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




template<class T>
class WatchingQueue {
public:
    WatchingQueue() = default;
    WatchingQueue(WatchingQueue<T>&&) = default;
    WatchingQueue(const WatchingQueue<T>&) = delete;
    WatchingQueue& operator=(WatchingQueue<T>&&) = default;
    WatchingQueue& operator=(const WatchingQueue<T>&) = delete;

    typedef typename std::list<std::queue<T>>::iterator WatchHandle;
    ~WatchingQueue() {
        SCHECK(_que.empty()) << "still have watcher";
    }
    WatchHandle watch() {
        std::lock_guard<std::mutex> lock(_mutex);  
        return _que.emplace(_que.end());
    }
    void cancel(WatchHandle handle) {
        std::lock_guard<std::mutex> lock(_mutex);  
        _que.erase(handle);
    }
    void push(const T& msg) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_closed) {
                SLOG(WARNING) << "push after close, the pop behavior is undefined.";
            }
            for (auto& que: _que) {
                que.push(msg);
            }
        }
        _cv.notify_all();
    }
    bool pop(WatchHandle handle, T& msg) {
        std::unique_lock<std::mutex> lock(_mutex);
        for(;;) {
            if (!handle->empty()) {
                msg = handle->front();
                handle->pop();
                return true;
            }
            if (_closed) {
                return false;
            }
            _cv.wait(lock);
        }
    }
    void close() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _closed = true;            
        }
        _cv.notify_all();
    }
private:
    bool _closed = false;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::list<std::queue<T>> _que;
};


template<class T>
class MessageWatcher;

template<class T>
class MessageDelegate {
public:
    MessageDelegate(MessageDelegate<T>&&) = default;
    MessageDelegate(const MessageDelegate<T>&) = delete;
    MessageDelegate& operator=(MessageDelegate<T>&&) = default;
    MessageDelegate& operator=(const MessageDelegate<T>&) = delete;
    MessageDelegate() {
        _watching = std::make_shared<WatchingQueue<T>>();
    }
    ~MessageDelegate() {
        _watching->close();
    }
    void send(const T& msg) { 
        _watching->push(msg);
    }
    void close() { 
        _watching->close();
    }
private:
    std::shared_ptr<WatchingQueue<T>> _watching;
    friend MessageWatcher<T>;
};

template<class T>
class MessageWatcher {
public:
    MessageWatcher(const MessageDelegate<T>& delegate)
        : _watching(delegate._watching) {
        _handle = _watching->watch();
    }
    ~MessageWatcher() {
        if (_watching) {
            _watching->cancel(_handle);
        }
    }
    MessageWatcher() = default;
    MessageWatcher(MessageWatcher<T>&&) = default;
    MessageWatcher(const MessageWatcher<T>&) = delete;
    MessageWatcher& operator=(MessageWatcher<T>&&) = default;
    MessageWatcher& operator=(const MessageWatcher<T>&) = delete;
    bool receive(T& msg) {
        return _watching->pop(_handle, msg);
    }
private:
    std::shared_ptr<WatchingQueue<T>> _watching;
    typename WatchingQueue<T>::WatchHandle _handle;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif
