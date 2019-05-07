
#include "common/include/RpcSocket.h"

namespace paradigm4 {
namespace pico {

bool RpcSocket::send_msg(RpcMessage&& msg) {
    int64_t sz = _sending_queue_size.fetch_add(1, std::memory_order_acq_rel);
    if (sz == 0) {
        int cnt = 1;
        for (;;) {
            RpcMessage prefetch_msg;
            while (_sending_queue.pop(prefetch_msg)) {
                if (cnt > 0) {
                    if (!send_rpc_message(std::move(msg), true)) {
                        return false;
                    }
                }
                msg = std::move(prefetch_msg);
                ++cnt;
            }
            if (cnt > 0) {
                if (!send_rpc_message(std::move(msg), false)) {
                    return false;
                }
            }
            sz = _sending_queue_size.fetch_add(-cnt, std::memory_order_acq_rel);
            if (sz == cnt) {
                break;
            } else {
                cnt = 0;
            }
        }
    } else {
        _sending_queue.push(std::move(msg));
    }
    return true;
}

bool RpcSocket::try_recv_msgs(std::function<void(RpcMessage&&)> func) {
    static size_t block_size = 256 * 1024;
    while (true) {
        if (_buffer.avaliable_size() == 0) {
            recv_buffer_t tmp;
            tmp.alloc(block_size);
            std::memcpy(tmp.cursor,
                _buffer.msg_cursor,
                _buffer.cursor - _buffer.msg_cursor);
            tmp.cursor += _buffer.cursor - _buffer.msg_cursor;
            _buffer = std::move(tmp);
        }

        ssize_t n = recv_nonblock(_buffer.cursor, _buffer.avaliable_size());

        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            errno = 0;
            return true;
        }

        if (n <= 0) {
            if (n == 0) {
                SLOG(INFO) << "peer close.";
            } else {
                PSLOG(ERROR) << "recv error." << fds()[0];
            }
            return false;
        }
        _buffer.cursor += n;

        rpc_head_t* msg_hd;

        for (;;) {
            if (_buffer.msg_cursor + sizeof(rpc_head_t) <= _buffer.cursor) {
                msg_hd = reinterpret_cast<rpc_head_t*>(_buffer.msg_cursor);
            } else {
                if ((_buffer.msg_cursor + sizeof(rpc_head_t)
                    > _buffer.end())) {
                    recv_buffer_t tmp;
                    tmp.alloc(block_size);
                    std::memcpy(tmp.cursor,
                        _buffer.msg_cursor,
                        _buffer.cursor - _buffer.msg_cursor);
                    tmp.cursor += _buffer.cursor - _buffer.msg_cursor;
                    _buffer = std::move(tmp);
                }
                break;
            }
            char* expected_msg_end = _buffer.msg_cursor + msg_hd->msg_size();

            if (expected_msg_end <= _buffer.cursor) {
                func({_buffer.msg_cursor, _buffer.ptr});
                _buffer.msg_cursor = expected_msg_end;
            } else {
                if (expected_msg_end > _buffer.end()) {
                    recv_buffer_t tmp;
                    tmp.alloc(std::max(msg_hd->msg_size(), block_size));
                    std::memcpy(tmp.cursor,
                          _buffer.msg_cursor,
                          _buffer.cursor - _buffer.msg_cursor);
                    tmp.cursor += _buffer.cursor - _buffer.msg_cursor;
                    _buffer = std::move(tmp);
                }
                break;
            }
        }
    }
}

void RpcSocket::recv_buffer_t::alloc(size_t sz) {
    auto alloc = PicoAllocator<char>();
    
    ptr = alloc_shared(alloc, sz);
    size = sz;
    msg_cursor = cursor = ptr.get();
}

int RpcAcceptor::bind_on_random_port(const std::string& bind_ip) {
    for (;;) {
        uint16_t port = 2048 + rand() % (65536 - 2048);
        std::string ep = bind_ip + ":" + std::to_string(port);
        if (bind(ep)) {
            SLOG(INFO) << "bind on " << ep;
            return 0;
        }
        if (errno != EADDRINUSE) {
            PSLOG(WARNING) << "can not bind on " << ep;
            return -1;
        }
    }
}
} // namespace pico
} // namespace paradigm4

