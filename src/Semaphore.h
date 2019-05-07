#ifndef PARADIGM4_PICO_COMMON_SEMAPHORE_H
#define PARADIGM4_PICO_COMMON_SEMAPHORE_H

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>

namespace paradigm4 {
namespace pico {

inline void pico_big_yield() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

class Semaphore {
public:
    Semaphore() : _count(0) {}

    Semaphore(unsigned count) : _count(count) {}

    Semaphore(const Semaphore&) = delete;

    void set(unsigned count) { // will be UNSAFE when called AFTER calling wait/notify
        std::unique_lock<std::mutex> mlock(_mutex);
        _count = count;
        _cond.notify_all();
    }

    unsigned avail_num() { // UNSAFE after return
        std::unique_lock<std::mutex> mlock(_mutex);
        return _count;
    }

    template<class Rep, class Period>
    bool acquire_for(const std::chrono::duration<Rep, Period>& rel_time) {
        std::unique_lock<std::mutex> mlock(_mutex);
        bool is_ok = _cond.wait_for(mlock, rel_time, [this]() {return _count > 0;});
        if (is_ok) {
            --_count;
            return true;
        }
        return false;
    }

    void acquire() {
        std::unique_lock<std::mutex> mlock(_mutex);
        _cond.wait(mlock, [this]() {return _count > 0;});
        --_count;
    }

    void release() {
        std::unique_lock<std::mutex> mlock(_mutex);
        ++_count;
        _cond.notify_one();
    }

    void release(unsigned num) {
        if (__builtin_expect(num == 0, 0)) {
            return;
        }
        std::unique_lock<std::mutex> mlock(_mutex);
        _count += num;
        for (unsigned i = 0; i < num; i++) {
            _cond.notify_one();
        }
    }

    bool try_acquire() {
        std::unique_lock<std::mutex> mlock(_mutex, std::defer_lock);
        if (!mlock.try_lock()) {
            return false;
        }
        if (_count) {
            --_count;
            return true;
        } else {
            return false;
        }
    }

private:
    unsigned int _count;
    std::mutex _mutex;
    std::condition_variable _cond;
};

} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_SEMAPHORE_H
