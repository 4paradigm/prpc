#ifndef PARADIGM4_PICO_CORE_ASYNC_WATCHER_H
#define PARADIGM4_PICO_CORE_ASYNC_WATCHER_H

#include "pico_log.h"
#include <mutex>
#include <condition_variable>
#include <list>
#include <queue>

namespace paradigm4 {
namespace pico {
namespace core {


class AsyncWatcher {
public:
    typedef uint64_t Atomic;
    void notify() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            ++_version;
        }
        _cv.notify_all();
    }
    Atomic watch() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _version;
    }
    Atomic watch(Atomic atom) {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this, atom](){ return _version != atom; });
        return _version;
    }
    Atomic watch(Atomic atom, int timeout) {
        SCHECK(timeout >= 0);
        std::unique_lock<std::mutex> lock(_mutex);
        if (timeout > 0) {
            std::chrono::milliseconds dur(timeout);
            _cv.wait_for(lock, dur, [this, atom](){ return _version != atom; });
        }
        return _version;
    }
    template<class Pred>
    void wait(Pred pred) {
        for (Atomic atom = watch(); !pred(); atom = watch(atom)) {}
    }
    template<class Pred>
    bool wait(int timeout, Pred pred) {
        SCHECK(timeout >= 0);
        Atomic atom = watch();
        auto begin = std::chrono::high_resolution_clock::now();
        while (!pred()) {
            auto dur = std::chrono::high_resolution_clock::now() - begin;
            int pass = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
            if (pass >= timeout) {
                return false;
            }
            Atomic next = watch(atom, timeout - pass);
            if (atom == next) {
                return false;
            }
            atom = next;
        }
        return true;
    }
private:
    Atomic _version; 
    std::mutex _mutex;
    std::condition_variable _cv;
};



} // namespace core
} // namespace pico
} // namespace paradigm4

#endif
