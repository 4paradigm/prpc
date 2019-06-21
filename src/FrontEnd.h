#ifndef PARADIGM4_PICO_FRONTEND_H
#define PARADIGM4_PICO_FRONTEND_H

#include <mutex>
#include <memory>
#include <atomic>
#include "Master.h"
#include "MpscQueue.h"

namespace paradigm4 {
namespace pico {
namespace core {


class RpcContext;

constexpr int FRONTEND_DISCONNECT = 1;
constexpr int FRONTEND_CONNECT = 2;
constexpr int FRONTEND_EPIPE = 1 + 4;
constexpr int FRONTEND_KEEP_WRITING = 2 + 8;

class FrontEnd {
    friend RpcContext;
public:
    int state() const {
        return _state.load(std::memory_order_acquire);
    }

    void set_state(int state) {
        _state.store(state, std::memory_order_release);
    }

    CommInfo info() {
        return _info;
    }

    bool connect();

    bool send_msg_nonblock(RpcMessage&& msg);

    /*
     * 多线程会调用，确保只有一个线程
     * keep_writing 其他线程直接退出
     */
    void keep_writing();

    // thread safe, may call ctx->send_msg when flush pending
    void send_msg(RpcMessage&& msg);

    void epipe(bool nonblock);

    bool available() const {
        if ((state() & FRONTEND_EPIPE) == FRONTEND_EPIPE) {
            std::chrono::duration<double> diff
                  = std::chrono::system_clock::now() - _epipe_time;
            return diff.count() > 10;
        } else {
            return true;
        }
    }

    bool& is_client_socket() {
        return _is_client_socket;
    }

    std::atomic_flag& is_sending() {
        return _is_sending;
    }

    bool handle_event(int fd,  std::function<void(RpcMessage&&)> func) {
        return _socket->handle_event(fd, func);
    }

private:
    std::mutex _mu;
    std::unique_ptr<RpcSocket> _socket;
    CommInfo _info;
    bool _is_client_socket;
    bool _is_use_rdma;
    int _epfd = -1;

    std::atomic<int> _state = {FRONTEND_DISCONNECT};
    std::chrono::time_point<std::chrono::system_clock> _epipe_time;

    RpcContext* _ctx;
    std::thread _background_thread;

    std::atomic_flag _is_sending;

    RpcMessage _sending_msg;
    RpcMessage::byte_cursor _it1, _it2;
    std::atomic<int> _sending_queue_size;
    MpscQueue<RpcMessage> _sending_queue;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif
