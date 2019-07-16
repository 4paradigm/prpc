#ifndef PARADIGM4_PICO_CORE_SPINLOCK_H
#define PARADIGM4_PICO_CORE_SPINLOCK_H

#include <algorithm>
#include <atomic>
#include <thread>
#include <random>

#include <boost/fiber/detail/cpu_relax.hpp>
#include "macro.h"

namespace paradigm4 {
namespace pico {
namespace core {

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
 * 优先级：lock_shared > upgrade > lock_shared_low > lock
 * 如果只使用lock_shared_low和upgrade可避免Writer starvation
 * upgrade过程中不会有人lock成功，但是可能会有其他读锁upgrade成功
 * 其他情况如果一直有Reader，则会导致Writer starvation
 */
class RWSpinLock {
    enum : int32_t { UPGRADE = 1 << 16, READER = 2, WRITER = 1 };
    enum : int32_t { MAX_TESTS = 1 << 10, MAX_COLLISION = 10 };

public:
    constexpr RWSpinLock() : _(0) {}

    RWSpinLock(RWSpinLock const&) = delete;
    RWSpinLock& operator=(RWSpinLock const&) = delete;

    void lock() {
        ttas([this](int value){ return test_lock(value); }, 
             [this](int value){ return try_lock(value);  });
    }

    void lock_shared() {
        ttas([this](int value){ return test_lock_shared(value); }, 
             [this](int value){ return try_lock_shared(value);  });
    }

    void lock_shared_low() {
        ttas([this](int value){ return test_lock_shared_low(value); }, 
             [this](int value){ return try_lock_shared_low(value);  });
    }

    // 读变写
    void upgrade() {
        _.fetch_add(UPGRADE, std::memory_order_release);
        ttas([this](int value){ return test_upgrade(value); }, 
             [this](int value){ return try_upgrade(value);  });        
    }

    void unlock() {
        _.fetch_xor(WRITER, std::memory_order_release);
    }

    void unlock_shared() {
        _.fetch_sub(READER, std::memory_order_release);
    }

    // 写变读
    void downgrade() {
        _.fetch_add(READER, std::memory_order_acquire);
        _.fetch_xor(WRITER, std::memory_order_release);
    }

    // 尝试把write flag变成true，如果没有任何thread hold这个锁，则lock成功
    bool try_lock() {
        int32_t value = bits();
        if (unlikely(!test_lock(value))) {
            return false;
        }
        return try_lock(value);
    }

    // 尝试增加一个reader，如果此时有writer则把reader退回并返回false
    bool try_lock_shared() {
        int32_t value = bits();
        if (unlikely(!test_lock_shared(value))) {
            return false;
        }
        return try_lock_shared(value);
    }

    bool try_lock_shared_low() {
        int32_t value = bits();
        if (unlikely(!test_lock_shared_low(value))) {
            return false;
        }
        return try_lock_shared_low(value);
    }

    // 有且仅有一个在Reader的时候，upgrade成功，和upgrade逻辑不同
    bool try_upgrade() {
        int32_t expect = READER;
        return _.compare_exchange_weak(expect, WRITER, std::memory_order_acquire);
    }

    // for debug
    int32_t bits() const {
        return _.load(std::memory_order_acquire);
    }

private:
    template<class Tester, class TryLocker>
    void ttas(Tester tester, TryLocker try_locker) {
        for (int collisions = 0;; ++collisions) {
            int value = bits();
            for (int tests = 0; unlikely(!tester(value)); ++tests) {
                if (tests < MAX_TESTS) {
                    cpu_relax();
                } else {
                    static constexpr std::chrono::microseconds us0{0};
                    std::this_thread::sleep_for(us0);
                }
                value = bits();
            }
            if (likely(try_locker(value))) {
                return;
            }
        }
    }

    // value 是否满足对应加锁条件
    bool test_lock(int value) {
        return value == 0;
    }
    bool test_lock_shared(int value) {
        return (value & WRITER) == 0;
    }
    bool test_lock_shared_low(int value) {
        return value < UPGRADE && (value & WRITER) == 0;
    }
    bool test_upgrade(int value) {
        return (value & WRITER) == 0 && (value / UPGRADE) == (value % UPGRADE / READER);
    }


    // 假设value已经经过了test
    bool try_lock(int value) {
        return _.compare_exchange_weak(value, WRITER, std::memory_order_acq_rel);
    }
    bool try_lock_shared(int) {
        int32_t value = _.fetch_add(READER, std::memory_order_acq_rel);
        if (unlikely(!test_lock_shared(value))) {
            _.fetch_sub(READER, std::memory_order_release);
            return false;
        }
        return true;
    }
    bool try_lock_shared_low(int) {
        int32_t value = _.fetch_add(READER, std::memory_order_acq_rel);
        if (unlikely(!test_lock_shared_low(value))) {
            _.fetch_sub(READER, std::memory_order_release);
            return false;
        }
        return true;
    }
    bool try_upgrade(int value) {
        return _.compare_exchange_weak(value, 
              value - UPGRADE - READER + WRITER,
              std::memory_order_acq_rel);
    }
    std::atomic<int32_t> _;
    char _pad[60] = {0};
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

template <class T>
class upgrade_guard {
public:
    upgrade_guard(T& t) : lk(&t) {
        lk->upgrade();
    }

    ~upgrade_guard() {
        lk->downgrade();
    }
    
private:
    T* lk;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_SPINLOCK_H
