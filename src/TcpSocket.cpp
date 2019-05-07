#include "common/include/TcpSocket.h"
#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

namespace paradigm4 {
namespace pico {

TcpConfig TcpSocket::_tcp_config;
bool TcpSocket::_use_tcp_config = false;

TcpSocket::TcpSocket() : RpcSocket() {
    _fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    PSCHECK(_fd >= 0);
    set_sockopt(_fd);
}

TcpSocket::TcpSocket(int fd) : RpcSocket() {
    _fd = fd;
    set_sockopt(_fd);
}

void TcpSocket::set_sockopt(int fd) {
    int nodelay = 1;
    PSCHECK(::setsockopt(
                fd, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(int))
            == 0);
    int keepalive = 1;
    PSCHECK(::setsockopt(
                fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive))
            == 0);
    if (_use_tcp_config && _tcp_config.keepalive_time != -1) {
        int optval = _tcp_config.keepalive_time;
        PSCHECK(::setsockopt(
                fd, SOL_TCP, TCP_KEEPIDLE, &optval, sizeof(optval))
            == 0);
    }
    if (_use_tcp_config && _tcp_config.keepalive_intvl != -1) {
        int optval = _tcp_config.keepalive_intvl;
        PSCHECK(::setsockopt(
                fd, SOL_TCP, TCP_KEEPINTVL, &optval, sizeof(optval))
            == 0);
    }
    if (_use_tcp_config && _tcp_config.keepalive_probes != -1) {
        int optval = _tcp_config.keepalive_probes;
        PSCHECK(::setsockopt(
                fd, SOL_TCP, TCP_KEEPCNT, &optval, sizeof(optval))
            == 0);
    }
}

bool TcpSocket::try_recv_pending(std::function<void(RpcMessage&&)> func) {
    while (!_pending_msgs.empty()) {
        auto& msg = _pending_msgs.front();
        size_t size = msg._data[_block_id].length - _recieved_size;
        if (msg._data[_block_id].length < MIN_ZERO_COPY_SIZE || size == 0) {
            ++_block_id;
            _recieved_size = 0;
            if (_block_id == msg.head()->extra_block_count) {
                func(std::move(msg));
                _pending_msgs.pop_front();
                _block_id = 0;
            }
            continue;
        }
        char* ptr = msg._data[_block_id].data + _recieved_size;
        int ret = retry_eintr_call(
              ::recv, _fd2, ptr, size, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (ret <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                return false;
            }
        } else {
            _recieved_size += ret;
        }
    }
    return true;
}

bool TcpSocket::connect(const std::string& endpoint,
      const std::string& info,
      int64_t magic) {
    sockaddr_in addr = parse_rpc_endpoint(endpoint);
    int ret = 0;
    auto starttm = std::chrono::steady_clock::now();
    for (int i = 1; ;) {
        int ret = retry_eintr_call(
              ::connect, _fd, (sockaddr*)&addr, sizeof(addr));
        if (ret == 0)
            break;
        auto dur = std::chrono::steady_clock::now() - starttm;
        int sec = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
        if (_use_tcp_config && _tcp_config.connect_timeout >= 0 && 
                sec >= _tcp_config.connect_timeout) {
            PSLOG(WARNING) << "connect failed endpoint: " << endpoint
                    << ", exit connect";
            return false;
        }
        if (i < 32)
            i *= 2;
        PSLOG(WARNING) << "connect failed endpoint: " << endpoint
                << ", sleep for " << i << " seconds.";
        ::sleep(i);
    }

    sockaddr_in local_addr;
    socklen_t len = sizeof(sockaddr_in);
    ret = getsockname(_fd, (struct sockaddr*)&local_addr, &len);
    PSCHECK(ret == 0);
    local_addr.sin_port = htons(0);

    int accept_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    PSCHECK(accept_fd > 0);
    PSCHECK(::bind(accept_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) == 0) << "bind failed.";
    retry_eintr_call(::listen, accept_fd, 1);
    PSCHECK(getsockname(accept_fd, (struct sockaddr*)&local_addr, &len) == 0);

    SLOG(INFO) << "temporal bind local addr is  " << inet_ntoa(local_addr.sin_addr) << ":"
               << ntohs(local_addr.sin_port);
    _endpoint = inet_ntoa(local_addr.sin_addr);
    _endpoint += ":" + std::to_string(ntohs(local_addr.sin_port));

    int64_t meta[2] = {magic, (int64_t)info.length()};
    if (retry_eintr_call(
              ::send, _fd, (char*)&meta[0], sizeof(meta), MSG_NOSIGNAL)
          != sizeof(meta)) {
        PSLOG(INFO) << "connect failed endpoint and ret is : " << endpoint
                    << " " << ret;
        return false;
    }

    if (retry_eintr_call(
              ::send, _fd, (char*)info.c_str(), info.size(), MSG_NOSIGNAL)
          != (int)info.size()) {
        PSLOG(INFO) << "connect failed endpoint and ret is : " << endpoint
                    << " " << ret;
        return false;
    }

    if (retry_eintr_call(::send, _fd, (char*)&local_addr, sizeof(local_addr), MSG_NOSIGNAL)
          != sizeof(addr)) {
        PSLOG(INFO) << "connect failed endpoint and ret is : " << endpoint
                    << " " << ret;
        return false;
    }

    int temp_flags = fcntl(accept_fd, F_GETFL);
    fcntl(accept_fd, F_SETFL, temp_flags | O_NONBLOCK);
    pollfd pfds = {accept_fd, POLLIN | POLLPRI, 0};
    starttm = std::chrono::steady_clock::now();
    while (true) {
        auto dur = std::chrono::steady_clock::now() - starttm;
        int ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
        ms = 600000 - ms;
        if (ms <= 0) {
            SLOG(WARNING) << "temporal socket accept timeout";
            return false;
        }
        int ret = poll(&pfds, 1, ms);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            PSLOG(WARNING) << "temporal socket poll accept failed";
            return false;
        } else if (ret == 0) {
            SLOG(WARNING) << "temporal socket accept timeout";
            return false;
        }
        break;
    }

    sockaddr_in remote_addr;
    _fd2 = ::accept4(accept_fd, (sockaddr*)&remote_addr, &len, SOCK_CLOEXEC);
    if (_fd2 == -1) {
        PSLOG(WARNING) << "temporal socket accept failed";
        return false;
    }
    set_sockopt(_fd2);
    ::close(accept_fd);

    SLOG(INFO) << "close temporal bind.";
    return true;
}

std::string TcpSocket::endpoint() {
    return _endpoint;
}

bool TcpSocket::accept(std::string& info) {
    int64_t meta[2];
    if (retry_eintr_call(
              ::recv, _fd, (char*)&meta[0], sizeof(meta), MSG_NOSIGNAL)
          != sizeof(meta)) {
        return false;
    }
    int64_t magic = meta[0];
    int64_t len = meta[1];

    if (magic != 0) {
        SLOG(WARNING) << "magic not correct. " << magic;
        return false;
    }

    //PSCHECK(magic == 0) << "magic : " << magic << " connect info len : " << meta;

    info.resize(len);
    if (retry_eintr_call(::recv, _fd, (char*)info.data(), len, MSG_NOSIGNAL)
          != len) {
        return false;
    }

    sockaddr_in addr;
    if (retry_eintr_call(::recv, _fd, &addr, sizeof(addr), MSG_NOSIGNAL) != sizeof(addr)) {
        return false;
    }

    _fd2 = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    PSCHECK(_fd2 >= 0);
    set_sockopt(_fd2);

    auto starttm = std::chrono::steady_clock::now();
    for (int i = 1; ;) {
        int ret = retry_eintr_call(
              ::connect, _fd2, (sockaddr*)&addr, sizeof(addr));
        if (ret == 0)
            break;
        auto dur = std::chrono::steady_clock::now() - starttm;
        int sec = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
        if (_use_tcp_config && _tcp_config.connect_timeout >= 0 && 
                sec >= _tcp_config.connect_timeout) {
            PSLOG(WARNING) << "connect temporal failed, exit accept";
            return false;
        }
        if (i < 32)
            i *= 2;
        PSLOG(WARNING) << "connect temporal failed. sleep for " << i << " seconds.";
        ::sleep(i);
    }
    return true;
}

ssize_t TcpSocket::recv_nonblock(char* ptr, size_t size) {
    return retry_eintr_call(
          ::recv, _fd, ptr, size, MSG_NOSIGNAL | MSG_DONTWAIT);
}

inline bool _send_raw(int _fd, char* ptr, size_t size, bool more) {
    int flag = MSG_NOSIGNAL;
    if (more) {
        flag |= MSG_MORE;
    }
    size_t sent = 0;
    for (sent = 0; sent < size;) {
        ssize_t nbytes = retry_eintr_call(
              ::send, _fd, ptr + sent, size - sent, flag | MSG_NOSIGNAL);
        if (nbytes > 0) {
            sent += nbytes;
        } else {
            PSLOG(WARNING) << "fd " << _fd << " send error";
            return false;
        }
    }
    return true;
}

bool TcpSocket::send_rpc_message(RpcMessage&& msg, bool more) {
    msg.send(more,
          [this](char* ptr, size_t size, bool more) {
              return _send_raw(_fd, ptr, size, more);
          },
          [this](char* ptr, size_t size, bool more) {
              return _send_raw(_fd2, ptr, size, more);
          });
    return true;
}

bool TcpSocket::recv_rpc_messages(
      std::vector<paradigm4::pico::RpcMessage>& rmsgs) {
    rmsgs.clear();
    bool func_called = false;
    bool socket_alive = false;

    auto func = [&func_called, &rmsgs](RpcMessage&& msg) {
        rmsgs.push_back(std::move(msg));
        func_called = true;
    };

    pollfd fds{in_fd(), POLLIN | POLLPRI, 0};

    while (!func_called) {
        socket_alive = handle_event(_fd, func);

        if (!socket_alive) {
            return false;
        }

        if (!func_called) {
            retry_eintr_call(::poll, &fds, 1, -1);
        }
    }

    return socket_alive;
}

TcpAcceptor::TcpAcceptor() : RpcAcceptor() {
    _fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    PSCHECK(_fd != -1);
    _ep = "";
}

bool TcpAcceptor::bind(const std::string& endpoint) {
    SCHECK(_ep == "") << "already bind on " << _ep;
    sockaddr_in addr = parse_rpc_endpoint(endpoint);
    if (::bind(_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        _ep = endpoint;
        return true;
    } else {
        return false;
    }
}

int TcpAcceptor::listen(int backlog) {
    return retry_eintr_call(::listen, _fd, backlog);
}

std::unique_ptr<RpcSocket> TcpAcceptor::accept() {
    sockaddr_in remote_addr;
    socklen_t len = sizeof(remote_addr);
    int fd = ::accept4(_fd, (sockaddr*)&remote_addr, &len, SOCK_CLOEXEC);
    PSCHECK(fd != -1);
    SLOG(INFO) << "received a connection from "
               << inet_ntoa(remote_addr.sin_addr) << ":"
               << ntohs(remote_addr.sin_port) << " fd is : " << fd;
    return std::make_unique<TcpSocket>(fd);
}

} // namespace pico
} // namespace paradigm4

