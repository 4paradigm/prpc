#include "FrontEnd.h"
#include "SpinLock.h"
#include "RpcContext.h"

namespace paradigm4 {
namespace pico {
namespace core {

bool FrontEnd::connect() {
    lock_guard<std::mutex> _(_mu);
    if (_state.load(std::memory_order_acquire) & FRONTEND_DISCONNECT) {
        if (_is_use_rdma) {
#ifdef USE_RDMA
            _socket = std::make_unique<RdmaSocket>();
#else
            SLOG(FATAL) << "rdma not supported.";
#endif
        } else {
            _socket = std::make_unique<TcpSocket>();
        }
        BinaryArchive ar;
        ar << uint16_t(0);
        ar << _ctx->self();
        std::string info;
        info.resize(ar.readable_length());
        memcpy(&info[0], ar.buffer(), info.size());
        if (!_socket->connect(_info.endpoint, info, 0)) {
            set_state(FRONTEND_EPIPE);
            epipe(false);
            return false;
        }
        set_state(FRONTEND_CONNECT);
    }
    return true;
}

void FrontEnd::send_msg_nonblock(RpcMessage&& msg) {
    int sz = _sending_queue_size.fetch_add(1, std::memory_order_acq_rel);
    if (sz == 0) {
        // 等keep writing结束 可能可以去掉这句话
        for (;;) {
            int cnt = 0;
            bool more = true;
            // 只有一个线程能到这里
            while (more) {
                _sending_msg = std::move(msg);
                more = _sending_queue.pop(msg);
                _it1.cursor(_sending_msg);
                _it2.zero_copy_cursor(_sending_msg);
                ++cnt;
                if (!_socket->send_msg(_sending_msg, true, more, _it1, _it2)) {
                    epipe(true);
                    return;
                }
                if (_it1.has_next() || _it2.has_next()) {
                    // 对于RDMA的情况，一定走不到这里
                    SCHECK(!_is_use_rdma);
                    // 保证读锁连续
                    _ctx->_spin_lock.lock_shared();
                    std::thread tmp = std::thread(
                          &FrontEnd::keep_writing, this, cnt, more, std::move(msg));
                    tmp.detach();
                    return;
                }
            }
            sz = _sending_queue_size.fetch_add(-cnt, std::memory_order_acq_rel);
            if (sz == cnt) {
                break;
            } else {
                while (!_sending_queue.pop(msg));
            }
        }
    } else {
        _sending_queue.push(std::move(msg));
    }
}

void FrontEnd::send_msg(RpcMessage&& msg) {
    int sz = _sending_queue_size.fetch_add(1, std::memory_order_acq_rel);
    if (sz == 0) {
        for (;;) {
            int cnt = 0;
            bool more = true;
            // 只有一个线程能到这里
            while (more) {
                _sending_msg = std::move(msg);
                more = _sending_queue.pop(msg);
                _it1.cursor(_sending_msg);
                _it2.zero_copy_cursor(_sending_msg);
                ++cnt;
                if (!_socket->send_msg(_sending_msg, false, more, _it1, _it2)) {
                    epipe(true);
                    return;
                }
                if (_it1.has_next() || _it2.has_next()) {
                    SLOG(FATAL) << "fxxxxxxxxxxxxxxk!!!!!";
                    return;
                }
            }
            sz = _sending_queue_size.fetch_add(-cnt, std::memory_order_acq_rel);
            if (sz == cnt) {
                break;
            } else {
                while (!_sending_queue.pop(msg));
            }
        }
    } else {
        _sending_queue.push(std::move(msg));
    }
}

/*
 * 内部函数，外部保证只有一个线程调用
 */
void FrontEnd::epipe(bool nonblock) {
    {
        _ctx->remove_frontend_event(this);
    }
    set_state(FRONTEND_EPIPE);
    if (!_is_client_socket) {
        return;
    }
    _epipe_time = std::chrono::system_clock::now();
    _it1.reset();
    _it2.reset();
    _ctx->send_request(std::move(_sending_msg), nonblock);
    while (_sending_queue.pop(_sending_msg)) {
        _ctx->send_request(std::move(_sending_msg), nonblock);
    }
    _sending_queue_size.store(0, std::memory_order_release);
}

void FrontEnd::keep_writing(int cnt, bool more, RpcMessage&& msg) {
    if (_it1.has_next() || _it2.has_next()) {
        if (!_socket->send_msg(_sending_msg, false, more, _it1, _it2)) {
            epipe(false);
            return;
        }
    }
    for (;;) {
        while (more) {
            _sending_msg = std::move(msg);
            more = _sending_queue.pop(msg);
            _it1.cursor(_sending_msg);
            _it2.zero_copy_cursor(_sending_msg);
            ++cnt;
            if (!_socket->send_msg(_sending_msg, false, more, _it1, _it2)) {
                epipe(false);
                return;
            }
        }
        int sz = _sending_queue_size.fetch_add(-cnt, std::memory_order_acq_rel);
        if (sz == cnt) {
            break;
        } else {
            cnt = 0;
            while (!_sending_queue.pop(msg));
            more = true;
        }
    }
    _ctx->_spin_lock.unlock_shared();
}


} // namespace core
} // namespace pico
} // namespace paradigm4
