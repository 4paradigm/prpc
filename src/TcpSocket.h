#ifndef PARADIGM4_PICO_CORE_TCP_H
#define PARADIGM4_PICO_CORE_TCP_H

#include <algorithm>
#include <atomic>
#include <cstring>

#include "fetch_ip.h"
#include "RpcSocket.h"
#include "RpcMessage.h"
#include "SpscQueue.h"

namespace paradigm4 {
namespace pico {
namespace core {

struct TcpConfig {
    TcpConfig() = default;

    template<typename T>
    TcpConfig(const T& o) {
        keepalive_time = o.keepalive_time;
        keepalive_intvl = o.keepalive_intvl;
        keepalive_probes = o.keepalive_probes;
        connect_timeout = o.connect_timeout;
    }

    int keepalive_time;
    int keepalive_intvl;
    int keepalive_probes;
    int connect_timeout;
};

class TcpSocket : public RpcSocket {
public:
    TcpSocket();

    TcpSocket(int fd);

    ~TcpSocket() {
        if (_fd != -1)
            PSCHECK(::close(_fd) == 0);
        if (_fd2 != -1)
            PSCHECK(::close(_fd2) == 0);
    }

    static void set_tcp_config(const TcpConfig& config) {
        _use_tcp_config = true;
        _tcp_config = config;
    }

    static void set_sockopt(int fd);

    bool connect(const std::string& endpoint, const std::string& info, int64_t magic) override;

    std::string endpoint();

    std::vector<int> fds() override {
        return {_fd, _fd2};
    }

    int in_fd() {
        return _fd;
    }

    bool accept(std::string& info) override;

    ssize_t recv_nonblock(char* ptr, size_t size) override;

    bool try_recv_pending(std::function<void(RpcMessage&&)> func);

    virtual bool handle_event(int fd, std::function<void(RpcMessage&&)> func) override {
        if (fd == _fd2) {
            return try_recv_pending(func);
        } else {
            SCHECK(fd == _fd);
            auto tcp_func = [this, func](RpcMessage&& msg) {
                if (msg.head()->extra_block_count == 0) {
                    func(std::move(msg));
                } else {
                    _pending_msgs.push_back(std::move(msg));
                }
            };
            if (!try_recv_msgs(tcp_func)) {
                return false;
            }
            if (!try_recv_pending(func)) {
                return false;
            }
            return true;
        }
    }

    virtual bool send_msg(RpcMessage& msg,
          bool nonblock,
          bool more,
          RpcMessage::byte_cursor& it1,
          RpcMessage::byte_cursor& it2) override;

    bool recv_rpc_messages(std::vector<RpcMessage>& rmsgs);

    static TcpConfig _tcp_config;
private:

    // for recv   
    pico::core::deque<RpcMessage> _pending_msgs;
    size_t _block_id = 0, _recieved_size = 0;

    int _fd = -1;
    int _fd2 = -1;

    std::string _endpoint;

    static bool _use_tcp_config;
};

class TcpAcceptor : public RpcAcceptor {
public:
    TcpAcceptor();

    ~TcpAcceptor() {
        ::close(_fd);
    }

    TcpAcceptor(const TcpAcceptor&) = delete;
    TcpAcceptor(TcpAcceptor&&) = delete;
    TcpAcceptor& operator=(const TcpAcceptor&) = delete;
    TcpAcceptor& operator=(TcpAcceptor&&) = delete;

    const std::string& endpoint() override {
        return _ep;
    }

    bool bind(const std::string& endpoint) override;

    int listen(int backlog) override;

    std::unique_ptr<RpcSocket> accept() override;

    int fd() override {
        return _fd;
    }

private:
    int _fd;
    std::string _ep;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_TCP_H
