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
        std::unique_ptr<RpcSocket> socket;
        if (_is_use_rdma) {
#ifdef USE_RDMA
            socket = std::make_unique<RdmaSocket>();
#else
            SLOG(FATAL) << "rdma not supported.";
#endif
        } else {
            socket = std::make_unique<TcpSocket>();
        }
        BinaryArchive ar;
        ar << uint16_t(0);
        ar << _ctx->self();
        std::string info;
        info.resize(ar.readable_length());
        memcpy(&info[0], ar.buffer(), info.size());
        if (!socket->connect(_info.endpoint, info, 0)) {
            return false;
        }
        lock_guard<RWSpinLock> l(_ctx->_spin_lock);
        _socket = std::move(socket);
        _ctx->add_frontend_event(this);
        set_state(FRONTEND_CONNECT);
    }
    return true;
}

void FrontEnd::send_msg_nonblock(RpcMessage&& msg, std::shared_ptr<FrontEnd>& this_holder) {
    int sz = _sending_queue_size.fetch_add(1, std::memory_order_acq_rel);
    if (sz == 0) {
        // 只有一个线程能到这里
        _msg = std::move(msg);
        _more = true;
        int cnt = 0;
        _it1.reset();
        _it2.reset();
        if (state() & FRONTEND_DISCONNECT) {
            // 保证读锁连续
            //_ctx->_spin_lock.lock_shared();
            _ctx->async([this, cnt, this_holder](){
                keep_writing(cnt);
                //_ctx->_spin_lock.unlock_shared();
            });
            return;
        }
        for (;;) {
            while (_more) {
                _sending_msg = std::move(_msg);
                _more = _sending_queue.pop(_msg);
                ++cnt;
                _it1.cursor(_sending_msg);
                _it2.zero_copy_cursor(_sending_msg);
                if (!_socket->send_msg(_sending_msg, true, _more, _it1, _it2)) {
                    epipe(cnt, true);
                    return;
                }
                if (_it1.has_next() || _it2.has_next()) {
                    // 对于RDMA的情况，一定走不到这里
                    SCHECK(!_is_use_rdma);
                    // 保证读锁连续
                    //_ctx->_spin_lock.lock_shared();
                    _ctx->async([this, cnt, this_holder](){
                        keep_writing(cnt);
                        //_ctx->_spin_lock.unlock_shared();
                    });
                    return;
                }
            }
            sz = _sending_queue_size.fetch_sub(cnt, std::memory_order_acq_rel);
            if (sz == cnt) {
                return;
            } else {
                while (!_sending_queue.pop(_msg));
                _more = true;
                cnt = 0;
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
        _it1.reset();
        _it2.reset();
        keep_writing(0);
    } else {
        _sending_queue.push(std::move(msg));
    }
}

/*
 * 内部函数，外部保证只有一个线程调用
 */
void FrontEnd::epipe(int cnt, bool nonblock) {
    set_state(FRONTEND_EPIPE);
    _ctx->remove_frontend_event(this);
    // 对于server socket，等待重连时构造新的FrontEnd
    if (!_is_client_socket) {
        return;
    }
    _epipe_time = std::chrono::system_clock::now();
    _it1.reset();
    _it2.reset();

    _ctx->send_request(std::move(_sending_msg), nonblock);
    if (_more) {
        _ctx->send_request(std::move(_msg), nonblock);
        ++cnt;
    }
    while (_sending_queue_size.fetch_sub(cnt) != cnt) {
        while (!_sending_queue.pop(_sending_msg));
        _ctx->send_request(std::move(_sending_msg), nonblock);
        cnt = 1;
    }
    set_state(FRONTEND_DISCONNECT | FRONTEND_EPIPE);
}

void FrontEnd::keep_writing(int cnt) {
    if (state() & FRONTEND_DISCONNECT) {
        if (!connect()) {
            _sending_msg = std::move(_msg);
            _more = _sending_queue.pop(_msg);
            ++cnt;
            epipe(cnt, false);
            return;
        }
    }
    if (_it1.has_next() || _it2.has_next()) {
        if (!_socket->send_msg(_sending_msg, false, _more, _it1, _it2)) {
            epipe(cnt, false);
            return;
        }
    }
    for (;;) {
        while (_more) {
            _sending_msg = std::move(_msg);
            _more = _sending_queue.pop(_msg);
            ++cnt;
            _it1.cursor(_sending_msg);
            _it2.zero_copy_cursor(_sending_msg);
            if (!_socket->send_msg(_sending_msg, false, _more, _it1, _it2)) {
                epipe(cnt, false);
                return;
            }
            if (_it1.has_next() || _it2.has_next()) {
                SLOG(FATAL) << "fxxxxxxxxxxxxxxk!!!!!";
                return;
            }
        }
        int sz = _sending_queue_size.fetch_sub(cnt, std::memory_order_acq_rel);
        // 此时已有其他线程可能会进来，所以cnt必须是局部变量
        if (sz == cnt) {
            return;
        } else {
            while (!_sending_queue.pop(_msg));
            _more = true;
            cnt = 0;
        }
    }
}


} // namespace core
} // namespace pico
} // namespace paradigm4
