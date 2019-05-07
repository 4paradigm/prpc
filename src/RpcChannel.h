#ifndef PARADIGM4_PICO_COMMON_RPCCHANNEL_H
#define PARADIGM4_PICO_COMMON_RPCCHANNEL_H

#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <thread>

#include "MpscQueue.h"
#include "SpinLock.h"
#include "pico_log.h"

namespace paradigm4 {
namespace pico {

class RpcChannelID {
};

/*
 * 线程间通信的管道，一个线程send，另一个recv
 * unbounded buffer, 所以send，一定成功
 */
template <typename T>
class RpcChannel {
public:
    RpcChannel() {
        _id = AtomicID<RpcChannelID, int>::gen();
        if (unlikely(_id == -1)) {
            // -1 used for one-way request
            _id = AtomicID<RpcChannelID, int>::gen();
        }
        _fd = eventfd(0, EFD_SEMAPHORE);
        PSCHECK(_fd >= 0) << "no fd";
        _size    = 0;
    }

    void terminate() {
        int64_t _ = 1;
        SCHECK(::write(_fd, &_, sizeof(int64_t)) == sizeof(int64_t));
    }

    ~RpcChannel() {
        ::close(_fd);
    }

    void send(T&& item) {
        _q.push(std::move(item));
        int64_t sz = _size.fetch_add(1, std::memory_order_acq_rel);
        if (sz == -1) {
            int64_t _ = 1;
            PSCHECK(::write(_fd, &_, sizeof(int64_t)) == sizeof(int64_t));
        }
    }

    bool recv(T& item, int timeout, int spin_count = 128) {
        int64_t _ = 0;
        for (int i = 0; i < spin_count; ++i) {
            if (_q.pop(item)) {
                if (_size.fetch_add(-1, std::memory_order_relaxed) == 0) {
                    PSCHECK(::read(_fd, &_, sizeof(int64_t)) == sizeof(int64_t));
                }
                return true;
            }
        }
        if (timeout == 0) {
            return false;
        }
        auto sz = _size.fetch_add(-1, std::memory_order_release);
        SCHECK(sz >= 0);
        if (sz == 0) {
            if (timeout == -1) {
                SCHECK(::read(_fd, &_, sizeof(int64_t)) == sizeof(int64_t));
                if (_size.load(std::memory_order_acquire) >= 0) {
                    for (; !_q.pop(item); );
                    return true;
                } else {
                    return false;
                }
            } else {
                pollfd pfd{_fd, POLLIN | POLLPRI, 0};
                int nfd = poll(&pfd, 1, timeout);
                if (nfd == 0) {
                    _size.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }
                PSCHECK(::read(_fd, &_, sizeof(int64_t)) == sizeof(int64_t));
                return _q.pop(item);
            }
        } else {
            for (; !_q.pop(item); );
            return true;
        }
    }

    // use it for epoll
    int fd() {
        return _fd;
    }

    int id() {
        return _id;
    }

private:
    int _id;
    int _fd;
    char _pad_1[64];
    std::atomic<int64_t> _size;
    char _pad_2[64];
    MpscQueue<T> _q;
    char _pad_3[64];
};

} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_RPCCHANNEL_H
