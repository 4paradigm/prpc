#include "FrontEnd.h"
#include "SpinLock.h"
#include "RpcContext.h"

namespace paradigm4 {
namespace pico {
namespace core {

/*
 * 内部函数，外部保证只有一个线程调用
 */
bool FrontEnd::connect() {
    if (state() & FRONTEND_DISCONNECT) {
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
            return false;
        }
        upgrade_guard<RWSpinLock> l(_ctx->_spin_lock);
        _ctx->add_frontend_event(this);
        set_state(FRONTEND_CONNECT);
    }
    return true;
}

void FrontEnd::send_msg_nonblock(RpcMessage&& msg) {
    int sz = _sending_queue_size.fetch_add(1, std::memory_order_acq_rel);
    if (sz == 0) {
        // 只有一个线程能到这里
        _msg = std::move(msg);
        _more = true;
        _cnt = 0;
        _it1.reset();
        _it2.reset();
        if (state() & FRONTEND_DISCONNECT) {
            // 保证读锁连续
            _ctx->_spin_lock.lock_shared();
            _ctx->async([this](){
                keep_writing();
                _ctx->_spin_lock.unlock_shared();
            });
            return;
        }
        for (;;) {
            while (_more) {
                _sending_msg = std::move(_msg);
                _more = _sending_queue.pop(_msg);
                ++_cnt;
                _it1.cursor(_sending_msg);
                _it2.zero_copy_cursor(_sending_msg);
                if (!_socket->send_msg(_sending_msg, true, _more, _it1, _it2)) {
                    epipe(true);
                    return;
                }
                if (_it1.has_next() || _it2.has_next()) {
                    // 对于RDMA的情况，一定走不到这里
                    SCHECK(!_is_use_rdma);
                    // 保证读锁连续
                    _ctx->_spin_lock.lock_shared();
                    _ctx->async([this](){
                        keep_writing();
                        _ctx->_spin_lock.unlock_shared();
                    });
                    return;
                }
            }
            sz = _sending_queue_size.fetch_add(-_cnt, std::memory_order_acq_rel);
            if (sz == _cnt) {
                break;
            } else {
                while (!_sending_queue.pop(_msg));
                _more = true;
                _cnt = 0;
            }
        }
    } else {
        _sending_queue.push(std::move(msg));
    }
}

void FrontEnd::send_msg(RpcMessage&& msg) {
    int sz = _sending_queue_size.fetch_add(1, std::memory_order_acq_rel);
    if (sz == 0) {
        // 只有一个线程能到这里
        _msg = std::move(msg);
        _more = true;
        _cnt = 0;
        _it1.reset();
        _it2.reset();
        keep_writing();
    } else {
        _sending_queue.push(std::move(msg));
    }
}

/*
 * 内部函数，外部保证只有一个线程调用
 */
void FrontEnd::epipe(bool nonblock) {
    _ctx->remove_frontend_event(this);
    set_state(FRONTEND_EPIPE);
    if (!_is_client_socket) {
        return;
    }
    _epipe_time = std::chrono::system_clock::now();
    _it1.reset();
    _it2.reset();

    _ctx->send_request(std::move(_sending_msg), nonblock);
    if (_more) {
        _ctx->send_request(std::move(_msg), nonblock);
    }
    while (_sending_queue.pop(_sending_msg)) {
        _ctx->send_request(std::move(_sending_msg), nonblock);
    }
    _sending_queue_size.store(0, std::memory_order_release);
    set_state(FRONTEND_DISCONNECT | FRONTEND_EPIPE);
}

void FrontEnd::keep_writing() {
    if (state() & FRONTEND_DISCONNECT) {
        if (!connect()) {
            _sending_msg = std::move(_msg);
            _more = _sending_queue.pop(_msg);
            ++_cnt;
            epipe(false);
            return;
        }
    }
    if (_it1.has_next() || _it2.has_next()) {
        if (!_socket->send_msg(_sending_msg, false, _more, _it1, _it2)) {
            epipe(false);
            return;
        }
    }
    for (;;) {
        while (_more) {
            _sending_msg = std::move(_msg);
            _more = _sending_queue.pop(_msg);
            ++_cnt;
            _it1.cursor(_sending_msg);
            _it2.zero_copy_cursor(_sending_msg);
            if (!_socket->send_msg(_sending_msg, false, _more, _it1, _it2)) {
                epipe(false);
                return;
            }
            if (_it1.has_next() || _it2.has_next()) {
                SLOG(FATAL) << "fxxxxxxxxxxxxxxk!!!!!";
                return;
            }
        }
        int sz = _sending_queue_size.fetch_add(-_cnt, std::memory_order_acq_rel);
        if (sz == _cnt) {
            return;
        } else {
            while (!_sending_queue.pop(_msg));
            _more = true;
            _cnt = 0;
        }
    }
}


} // namespace core
} // namespace pico
} // namespace paradigm4
