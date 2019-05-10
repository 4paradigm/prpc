#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include "MasterClient.h"

namespace paradigm4 {
namespace pico {
namespace core {


TcpMasterClient::TcpMasterClient(const std::string& master_ep)
      : MasterClient("root"), _master_ep(std::move(master_ep)), _id_gen(0) {}

TcpMasterClient::~TcpMasterClient() {}

AsyncReturnV<RpcResponse> TcpMasterClient::send_request(RpcRequest&& req) {
    AsyncReturnV<RpcResponse> ret;
    int id = _id_gen.fetch_add(1) & 0x0FFFFFFF;
    req.head().rpc_id = id;
    {
        std::lock_guard<std::mutex> _(_lk);
        ret = _as_ret[id];
    }
    ret.reset();
    _tcp_socket->send_rpc_message(std::move(req), false);
    return ret;
}

bool TcpMasterClient::initialize() {
    SLOG(INFO) << "tcp master client initialize";
    PSCHECK(_tcp_socket = std::unique_ptr<TcpSocket>(new TcpSocket()));
    if (!_tcp_socket->connect(_master_ep, _master_ep, 0)) {
        SLOG(WARNING) << "connect to master failed!";
        return false;
    }
    _exit_fd = eventfd(0, EFD_SEMAPHORE);
    _listening_th = std::thread(&TcpMasterClient::listening, this);
    _cb_th = std::thread(&TcpMasterClient::run_cb, this);
    SLOG(INFO) << "tcp master client initialized";
    return MasterClient::initialize();
}

void TcpMasterClient::finalize() {
    MasterClient::finalize();
    SLOG(INFO) << "deregister tcp master client";
    RpcRequest req(-1);
    req << PicoMasterReqType::MASTER_CLIENT_FINALIZE;
    send_request(std::move(req)).wait();
    SLOG(INFO) << "tcp master client finalize";
    int64_t _ = 1;
    SCHECK(::write(_exit_fd, &_, sizeof(_)) == sizeof(int64_t));
    _cb_ch.terminate();
    _listening_th.join();
    _cb_th.join();
    _tcp_socket.reset();
    SLOG(INFO) << "tcp master client finalized";
}

std::string TcpMasterClient::endpoint() {
    return _tcp_socket->endpoint();
}

bool TcpMasterClient::connected() {
    return true;
}

bool TcpMasterClient::reconnect() {
    SLOG(FATAL) << "Cannot reconnect for tcp master";
    return false;
}

void TcpMasterClient::listening() {
    pollfd fds[2] = {
        {_tcp_socket->in_fd(), POLLIN | POLLPRI | POLLERR | POLLHUP | POLLRDHUP, 0},
        {_exit_fd, POLLIN | POLLPRI, 0},    
    };
    while (true) {
        PSCHECK(retry_eintr_call(::poll, fds, (nfds_t)2, -1) != -1);
        if (fds[0].revents != 0) {
            bool socket_alive = _tcp_socket->handle_event(fds[0].fd, [this](RpcMessage&& msg) {
                auto resp = std::make_shared<RpcResponse>(std::move(msg));
                if (resp->head().rpc_id == WATCHER_NOTIFY_RPC_ID) {
                    std::string path;
                    *resp >> path;
                    std::function<void()> func = [path, this]() {
                        notify_watchers(path);
                    };
                    _cb_ch.send(std::move(func));
                } else {
                    std::lock_guard<std::mutex> _(_lk);
                    _as_ret[resp->head().rpc_id].notify(resp);
                    _as_ret.erase(resp->head().rpc_id);
                }    
            });
            if (!socket_alive) {
                PSLOG(FATAL)
                      << "TcpMasterClient Socket failed which fd is : "
                      << fds[0].fd << " is going to close.";
            }
        }
        if (fds[1].revents != 0) {
            break;
        }
    }
}

void TcpMasterClient::run_cb() {
    std::function<void()> cb;
    while (_cb_ch.recv(cb, -1)) {
        cb();
    }
}

#define RPC_VOID

#define RPC_METHOD(method, input, output)              \
    RpcRequest req(-1);                                \
    req << PicoMasterReqType::method input;            \
    auto resp = send_request(std::move(req)).wait();   \
    MasterStatus ret = MasterStatus::ERROR;            \
    *resp >> ret;                                      \
    if (ret == MasterStatus::OK) {                     \
        *resp output;                                  \
    }                                                  \
    return ret;                                        \


MasterStatus TcpMasterClient::master_gen(const std::string& path, const std::string& value, std::string& gen, bool ephemeral) {
    RPC_METHOD(MASTER_GEN, << path << value << ephemeral, >> gen)
}

MasterStatus TcpMasterClient::master_add(const std::string& path, const std::string& value, bool ephemeral) {
    RPC_METHOD(MASTER_ADD, << path << value << ephemeral, RPC_VOID)
}

MasterStatus TcpMasterClient::master_set(const std::string& path, const std::string& value) {
    RPC_METHOD(MASTER_SET, << path << value, RPC_VOID)
}

MasterStatus TcpMasterClient::master_get(const std::string& path, std::string& value) {
    RPC_METHOD(MASTER_GET, << path << value, >> value)
}

MasterStatus TcpMasterClient::master_get(const std::string& path) {
    std::string value;
    return master_get(path, value);
}

MasterStatus TcpMasterClient::master_del(const std::string& path) {
    RPC_METHOD(MASTER_DEL, << path, RPC_VOID)
}

MasterStatus TcpMasterClient::master_sub(const std::string& path, std::vector<std::string>& children) {
    RPC_METHOD(MASTER_SUB, << path, >> children)
}


} // namespace core
} // namespace pico
} // namespace paradigm4
