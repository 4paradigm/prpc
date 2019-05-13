#ifndef PARADIGM4_PICO_CORE_RPC_SOCKET_H
#define PARADIGM4_PICO_CORE_RPC_SOCKET_H

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "MpscQueue.h"
#include "RpcMessage.h"

namespace paradigm4 {
namespace pico {
namespace core {

inline sockaddr_in parse_rpc_endpoint(const std::string& endpoint) {
    const char* ep = endpoint.c_str();
    const char* delim = std::strrchr(ep, ':');
    uint16_t port = 0;
    SCHECK(delim) << "no delim";
    std::string addr_str(ep, delim - ep);
    std::string port_str(delim + 1);
    if (port_str == "*" || port_str == "0") {
        port = 0;
    } else {
        port = (uint16_t)atoi(port_str.c_str());
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(addr_str.c_str());
    return addr;
}


/*
 * ip:port
 * 172.27.0.0.1:12345
 */
class RpcSocket {
public:
    RpcSocket() : _sending_queue_size(0) {}

    RpcSocket(const RpcSocket&) = delete;
    RpcSocket(RpcSocket&&) = delete;
    RpcSocket& operator=(const RpcSocket&) = delete;
    RpcSocket& operator=(const RpcSocket&&) = delete;

    virtual ~RpcSocket() {}


    bool send_msg(RpcMessage&& msg);

    // T 可以是RpcRequest和RpcResponse
    template <class T>
    bool send(T&& item) {
        return send_msg(RpcMessage(std::move(item)));
    }

    // 非阻塞, proxy调用
    virtual bool handle_event(int /*fd*/, std::function<void(RpcMessage&&)>) {
        SLOG(FATAL) << "not implemented.";
        return false;
    }

    virtual std::vector<int> fds() {
        return {};
    }

    // 用于连接后再次握手，确认magic，汇报角色
    virtual bool accept(std::string&) {
        return false;
    }

    // 用于连接后再次握手，确认magic，汇报角色
    virtual bool connect(const std::string& /*endpoint*/,
          const std::string& /*info*/,
          int64_t /*magic*/) {
        return false;
    }

    virtual bool send_rpc_message(RpcMessage&& msg, bool more = false) = 0;

protected:

    virtual ssize_t recv_nonblock(char* ptr, size_t size) = 0;

    bool try_recv_msgs(std::function<void(RpcMessage&&)>);

    struct recv_buffer_t {
        std::shared_ptr<char> ptr = nullptr;
        size_t size = 0;
        char* cursor = nullptr;
        char* msg_cursor = nullptr;

        void alloc(size_t sz);

        size_t avaliable_size() {
            return ptr.get() + size - cursor;
        }

        char* end() {
            return ptr.get() + size;
        }
    };

    recv_buffer_t _buffer;

private:
    std::atomic<int64_t> _sending_queue_size;
    MpscQueue<RpcMessage> _sending_queue;
};

class RpcAcceptor {
public:
    RpcAcceptor() {}
    virtual ~RpcAcceptor() {}

    RpcAcceptor(const RpcAcceptor&) = delete;
    RpcAcceptor(RpcAcceptor&&) = delete;
    RpcAcceptor& operator=(const RpcAcceptor&) = delete;
    RpcAcceptor& operator=(RpcAcceptor&&) = delete;

    virtual const std::string& endpoint() = 0;

    virtual bool bind(const std::string& endpoint) = 0;

    int bind_on_random_port(const std::string& bind_ip);

    virtual int listen(int backlog) = 0;

    virtual std::unique_ptr<RpcSocket> accept() = 0;
        
    virtual int fd() = 0;

};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_RPC_SOCKET_H
