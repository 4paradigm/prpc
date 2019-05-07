#ifndef PARADIGM4_PICO_COMMON_CHANNEL_ENTITY_H
#define PARADIGM4_PICO_COMMON_CHANNEL_ENTITY_H

#include <condition_variable>
#include <functional>
#include <deque>
#include <limits>
#include <mutex>
#include "pico_log.h"
#include "pico_memory.h"

#include "VirtualObject.h"

namespace paradigm4 {
namespace pico {

/*!
 * \brief class ChannelEntity is a deque of type T.
 *  read func get element from front and pop_front,
 *  write func put element into end of deque.
 *  write can not operate if channel is closed or reach its upper limits.
 */
template <class T>
class ChannelEntity : public VirtualObject {
public:
    typedef pico::deque<T, PicoAllocator<T>> buffer_type;

    ChannelEntity() {
        _reserved_data.reserve(RESERVED_SIZE);
    }

    explicit ChannelEntity(size_t capacity) {
        SCHECK(capacity >= 1);
        _capacity = capacity;
        _reserved_data.reserve(RESERVED_SIZE);
    }
    void set_capacity(size_t capacity) {
        std::lock_guard<std::mutex> mlock(_mutex);
        SCHECK(capacity >= 1);
        _capacity = capacity;
    }

    void set_capacity_unlocked(size_t capacity) {
        SCHECK(capacity >= 1);
        _capacity = capacity;
    }

    size_t capacity() {
        std::lock_guard<std::mutex> mlock(_mutex);
        return _capacity;
    }

    size_t capacity_unlocked() {
        return _capacity;
    }

    size_t size() {
        std::lock_guard<std::mutex> mlock(_mutex);
        return _data.size() + _reserved_data.size();
    }

    size_t size_unlocked() {
        return _data.size() + _reserved_data.size();
    }

    bool closed() {
        std::lock_guard<std::mutex> mlock(_mutex);
        return _closed;
    }

    void shared_open() {
        std::lock_guard<std::mutex> mlock(_mutex);
        _closed = false;
        _shared_count++;
    }

    void open() {
        std::lock_guard<std::mutex> mlock(_mutex);
        _closed = false;
    }

    void close() {
        std::lock_guard<std::mutex> mlock(_mutex);
        if (_shared_count && --_shared_count) {
            return;
        }
        _closed = true;
        _full_cond.notify_all();
        _empty_cond.notify_all();
    }

    bool full() {
        std::lock_guard<std::mutex> mlock(_mutex);
        return full_unlocked();
    }

    bool empty() {
        std::lock_guard<std::mutex> mlock(_mutex);
        return empty_unlocked();
    }

    bool full_unlocked() {
        return _data.size() + _reserved_data.size() >= _capacity;
    }

    bool empty_unlocked() {
        return _data.empty() && _reserved_data.empty();
    }

    bool read(T& val) {
        std::unique_lock<std::mutex> mlock(_mutex);
        _empty_cond.wait(mlock, [this]() { return !empty_unlocked() || _closed; });
        return read_unlocked(val);
    }

    bool try_read(T& val) {
        std::lock_guard<std::mutex> mlock(_mutex);
        if (empty_unlocked()) {
            return false;
        }
        return read_unlocked(val);
    }

    /*！
     * \brief read n elements into $vals, unless the channel is closed
     */
    size_t read(T* vals, size_t n) {
        if (n == 0) {
            return 0;
        }
        std::unique_lock<std::mutex> mlock(_mutex);
        for (size_t i = 0; i < n; ++i) {
            _empty_cond.wait(mlock, [this]() { return !empty_unlocked() || _closed; });
            if (!read_unlocked(vals[i])) {
                return i;
            }
        }
        return n;
    }

    bool read_unlocked(T& val) {
        if (empty_unlocked()) {
            return false;
        }
        if (!_reserved_data.empty()) {
            val = std::move(_reserved_data.front());
            _reserved_data.pop_back(); // pop_front
        } else {
            val = std::move(_data.front());
            _data.pop_front();
        }
        if (!_closed) {
            _oom_flag = false;
            _full_cond.notify_one();
        }
        return true;
    }

    bool write(const T& val) {
        std::unique_lock<std::mutex> mlock(_mutex);
        return safe_write_unlocked(mlock, val);
    }

    bool write(T&& val) {
        std::unique_lock<std::mutex> mlock(_mutex);
        return safe_write_unlocked(mlock, std::move(val));
    }

    //    bool write_front(T&& val) {
    //        std::unique_lock<std::mutex> mlock(_mutex);
    //        //if (_closed) return false;
    //        _data.push_front(std::move(val));
    //        _empty_cond.notify_one();
    //        return true;
    //    }

    bool try_write(const T& val) {
        std::lock_guard<std::mutex> mlock(_mutex);
        if (_closed || full_unlocked()) {
            return false;
        }
        try {
            return write_unlocked(val);
        } catch (std::bad_alloc) {
            return false;
        }
    }

    bool try_write(T&& val) {
        std::lock_guard<std::mutex> mlock(_mutex);
        if (_closed || full_unlocked()) {
            return false;
        }
        try {
            return write_move_unlocked(std::move(val));
        } catch (std::bad_alloc) {
            return false;
        }
    }

    size_t write(const T* vals, size_t n) { // write n elements, unless channel is closed
        if (n == 0) {
            return 0;
        }
        std::unique_lock<std::mutex> mlock(_mutex);
        for (size_t i = 0; i < n; ++i) {
            if (!safe_write_unlocked(mlock, vals[i])) {
                return i;
            }
        }
        return n;
    }

    size_t write_move(T* vals, size_t n) { // move
        if (n == 0) {
            return 0;
        }
        std::unique_lock<std::mutex> mlock(_mutex);
        for (size_t i = 0; i < n; ++i) {
            if (!safe_write_unlocked(mlock, std::move(vals[i]))) {
                return i;
            }
        }
        return n;
    }

    bool write_unlocked(const T& val) {
        if (_closed || full_unlocked()) {
            return false;
        }
        if (empty_unlocked()) {
            _reserved_data.push_back(val);
        } else {
            _data.push_back(val);
        }
        _empty_cond.notify_one();
        return true;
    }

    bool write_move_unlocked(T&& val) {
        if (_closed || full_unlocked()) {
            return false;
        }
        if (empty_unlocked()) {
            _reserved_data.push_back(std::move(val));
        } else {
            _data.push_back(std::move(val));
        }
        _empty_cond.notify_one();
        return true;
    }

    buffer_type& data() {
        return _data;
    }

    void for_each(std::function<void(const T&)> func) {
        std::unique_lock<std::mutex> mlock(_mutex);
        for (auto it = _reserved_data.begin(); it != _reserved_data.end(); ++it) {
            func(*it);
        }
        for (auto it = _data.begin(); it != _data.end(); ++it) {
            func(*it);
        }
    }

//protected:
    bool safe_write_unlocked(std::unique_lock<std::mutex>& mlock, const T& val) {
        while (true) {
            _full_cond.wait(
                mlock, [this]() { return (!_oom_flag && !full_unlocked()) || _closed; });
            try {
                return write_unlocked(val);
            } catch (std::bad_alloc) {
                _oom_flag = true;
                SLOG(WARNING) << "OOM in channel, current channel size="
                              << size_unlocked();
            }
        }
    }
    bool safe_write_unlocked(std::unique_lock<std::mutex>& mlock, T&& val) {
        while (true) {
            _full_cond.wait(
                mlock, [this]() { return (!_oom_flag && !full_unlocked()) || _closed; });
            try {
                return write_move_unlocked(std::move(val));
            } catch (std::bad_alloc) {
                _oom_flag = true;
                SLOG(WARNING) << "OOM in channel, current channel size="
                              << size_unlocked();
            }
        }
    }
    static const size_t RESERVED_SIZE = 1;
    std::mutex _mutex;
    buffer_type _data;
    std::vector<T> _reserved_data;
    size_t _shared_count = 0;
    bool _oom_flag       = false;
    bool _closed         = false; // if channel is closed, it'll be read-only
    size_t _capacity     = std::numeric_limits<size_t>::max();
    std::condition_variable _empty_cond;
    std::condition_variable _full_cond;
};

} // end of namespace pico
} // end of namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_CHANNEL_ENTITY_H
