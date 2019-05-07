#ifndef PARADIGM4_PICO_COMMON_SPINLOCK_H
#define PARADIGM4_PICO_COMMON_SPINLOCK_H

#include <algorithm>
#include <atomic>
#include <thread>

#include "macro.h"

namespace paradigm4 {
namespace pico {

class SpinLock {
public:
    SpinLock() {
        _.clear();
    }

    std::atomic_flag _;

public:
    bool try_lock() {
        return !_.test_and_set(std::memory_order_acquire);
    }

    void lock() {
        for (uint32_t count = 0; unlikely(!try_lock()); ++count) {
            if (count > 1000) {
                std::this_thread::yield();
            }
        }
    }

    void unlock() {
        _.clear(std::memory_order_release);
    }
};

/*
 * 非常轻量级的rwspinlock，
 * 写的优先级很低，如果一直有人Reader，那么会导致Writer starvation
 */
class RWSpinLock {
    enum : int32_t { READER = 2, WRITER = 1 };

public:
    constexpr RWSpinLock() : _(0) {}

    RWSpinLock(RWSpinLock const&) = delete;
    RWSpinLock& operator=(RWSpinLock const&) = delete;

    void lock() {
        for (uint32_t count = 0; unlikely(!try_lock()); ++count) {
            if (count > 1000) {
                std::this_thread::yield();
            }
        }
    }

    // 尝试把write flag变成true，如果没有任何thread hold这个锁，则lock成功
    bool try_lock() {
        int32_t expect = 0;
        return _.compare_exchange_weak(expect, WRITER, std::memory_order_acq_rel);
    }

    void unlock() {
        _.fetch_xor(WRITER, std::memory_order_release);
    }

    //增加一个reader，然后等Writer退出
    void lock_shared() {
        int32_t value = _.fetch_add(READER, std::memory_order_acquire);
        for (uint32_t count = 0; unlikely(value & WRITER); ++count) {
            value = _.load(std::memory_order_acquire);
            if (count > 1000) {
                std::this_thread::yield();
            }
        }
    }

    // 尝试增加一个reader，如果此时有writer则把reader退回并返回false
    bool try_lock_shared() {
        int32_t value = _.fetch_add(READER, std::memory_order_acquire);
        if (unlikely(value & WRITER)) {
            _.fetch_add(-READER, std::memory_order_release);
            return false;
        }
        return true;
    }

    // 减少一个reader
    void unlock_shared() {
        _.fetch_add(-READER, std::memory_order_release);
    }

    // 读变写
    void upgrade() {
        for (uint32_t count = 0; unlikely(!try_upgrade()); ++count) {
            if (count > 1000) {
                std::this_thread::yield();
            }
        }
    }

    // 有且仅有一个在Reader的时候，upgrade成功
    bool try_upgrade() {
        int32_t expect = READER;
        return _.compare_exchange_weak(expect, WRITER, std::memory_order_acq_rel);
    }

    // 写变读
    void downgrade() {
        _.fetch_add(READER, std::memory_order_acquire);
        _.fetch_xor(WRITER, std::memory_order_release);
    }

    // for debug
    int32_t bits() const {
        return _.load(std::memory_order_acquire);
    }

private:
    std::atomic<int32_t> _;
};

template <class T>
class shared_lock_guard {
public:
    shared_lock_guard(T& t) : lk(&t) {
        lk->lock_shared();
    }

    ~shared_lock_guard() {
        lk->unlock_shared();
    }

private:
    T* lk;
};

template <class T>
class lock_guard {
public:
    lock_guard(T& t) : lk(&t) {
        lk->lock();
    }

    ~lock_guard() {
        lk->unlock();
    }

private:
    T* lk;
};

} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_SPINLOCK_H
