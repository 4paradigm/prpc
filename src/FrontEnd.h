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
constexpr int FRONTEND_EPIPE = 4;

// 所有方法假设已有RpcContext读锁，并且需要一直持有读锁，防止FrontEnd被析构
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

    void send_msg_nonblock(RpcMessage&& msg);

    /*
     * 多线程会调用，确保只有一个线程
     * keep_writing 其他线程直接退出
     */
    void keep_writing(int cnt);

    // thread safe, may call ctx->send_msg when flush pending
    void send_msg(RpcMessage&& msg);

    void epipe(int cnt, bool nonblock);

    bool available() const {
        if (state() & FRONTEND_EPIPE) {
            if (state() & FRONTEND_DISCONNECT) {
                std::chrono::duration<double> diff
                    = std::chrono::system_clock::now() - _epipe_time;
                return diff.count() > 10;
            } else {
                return false;
            }
        } else {
            return true;
        }
    }

    bool& is_client_socket() {
        return _is_client_socket;
    }

    bool handle_event(int fd, std::function<void(RpcMessage&&)> func) {
        return _socket->handle_event(fd, func);
    }

private:
    std::mutex _mu; // for connect
    std::unique_ptr<RpcSocket> _socket;
    CommInfo _info;
    bool _is_client_socket;
    bool _is_use_rdma;
    int _epfd = -1;
    RpcContext* _ctx;
    std::chrono::time_point<std::chrono::system_clock> _epipe_time;

    char __pad__1[64];
    std::atomic<int> _state = {FRONTEND_DISCONNECT};
    char __pad__2[64];

// 发送线程的状态
    int _cnt = 0;
    bool _more = false;
    RpcMessage _sending_msg, _msg;
    RpcMessage::byte_cursor _it1, _it2;

    char __pad__3[64];
    std::atomic<int> _sending_queue_size;
    MpscQueue<RpcMessage> _sending_queue;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif
