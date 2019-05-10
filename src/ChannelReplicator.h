#ifndef PARADIGM4_PICO_COMMON_CHANNEL_REPLICATOR_H
#define PARADIGM4_PICO_COMMON_CHANNEL_REPLICATOR_H

#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <algorithm>

#include "pico_log.h"
#include "Channel.h"

namespace paradigm4 {
namespace pico {
namespace core {

template<class T>
class ChannelReplicator { // by one-slot open ring-buffer, can make another implementation later
public:
    ChannelReplicator() : ChannelReplicator(1024, 8){}

    ChannelReplicator(size_t chan_capacity) : ChannelReplicator(chan_capacity, 8){}

    ChannelReplicator(size_t chan_capacity, size_t cache_size) : 
        _input_chan(Channel<T>(chan_capacity)) {
            _data_cache_size = cache_size;
    }

    ~ChannelReplicator() {
        if (_inited) {
            finalize();
        }
    }

    Channel<T>& input_chan() { // this is NOT thread safe
        return _input_chan;
    }

    void add_output_chan(Channel<T>& chan) { // this is NOT thread safe
        SCHECK(!_inited);
        std::for_each(_output_chans.begin(), _output_chans.end(), [&chan](Channel<T>& o_ch) { 
                SCHECK(chan != o_ch);});
        chan.shared_open();
        _output_chans.push_back(chan);
    }
    
    void remove_output_chan(const Channel<T>& chan) { // this is NOT thread safe
        SCHECK(!_inited);
        for (size_t i = 0; i < _output_chans.size(); ++i) {
            if (_output_chans[i] == chan) {
                _output_chans[i].close();
                _output_chans.erase(_output_chans.begin() + i);
                break;
            }
        }
    }

    void initialize() {
        SCHECK(!_inited) << "already initialized";
        _inited = true;
        if (_input_chan.closed()) {
            _input_chan.open();
        }
        if (_output_chans.size() == 0) {
            _cache_thread = std::thread(&ChannelReplicator::dropping, this);
            return;
        } else if (_output_chans.size() == 1) {
            _input_chan = _output_chans[0];
            SCHECK(!_input_chan.closed()) << "output channel cannot be closed";
            return;
        }
        // can remove this CHECK later, I'm lazy
        SCHECK(_data_cache_size != 0) << "when there're multiple outputs cache size cannot be 0";
        _data_cache.resize(_data_cache_size + 1); // cache size need be configured later
        _cache_thread = std::thread(&ChannelReplicator::caching, this);
        _caching_closed = false; // must set false before starting rep_threads
        _rep_threads.resize(_output_chans.size());
        _read_pos.resize(_output_chans.size(), 0);
        for (size_t i = 0; i < _rep_threads.size(); i++) {
            _rep_threads[i] = std::thread(&ChannelReplicator::replicating, this, i);
        }
    }

    const std::vector<Channel<T>>& output_chans() const { // NOT thread safe
        return _output_chans;
    }

    void set_chan_capacity(size_t capacity) { // NOT thread safe
        SCHECK(!_inited);
        _input_chan.set_capacity(capacity);
    }

    void set_cache_size(size_t cache_size) { // NOT thread safe
        SCHECK(!_inited);
        SCHECK(cache_size != 0) << "cache size cannot be 0";
        _data_cache_size = cache_size;
    }

    size_t chan_capacity() {
        return _input_chan.capacity_unlocked();
    }

    size_t cache_size() {
        return _data_cache_size;
    }

    void close() {
        _input_chan.close();
    }

    void join() {
        SCHECK(_inited) << "not started";
        if (_output_chans.size() == 0) {
            _cache_thread.join();
            return;
        } else if (_output_chans.size() == 1) {
            return;
        }
        _cache_thread.join();
        for (auto& thread : _rep_threads) {
            thread.join();
        }
    }

    void finalize() {
        SCHECK(_inited) << "not started";
        _inited = false;
        _begin = 0;
        _end = 0;
        _data_cache.clear();
        _rep_threads.clear();
        _output_chans.clear();
        _read_pos.clear();
    }
    
private:
    void advance_pos(size_t& pos) {
        pos = (pos + 1) % _data_cache.size();
    }

    bool front_of(size_t pos1, size_t pos2) {
        return (pos1 + _data_cache.size() - _begin) % _data_cache.size()
            < (pos2 + _data_cache.size() - _begin) % _data_cache.size();
    }

    bool full() {
        return size() + 1 == _data_cache.size();
    }

    bool empty() {
        return _begin == _end;
    }

    size_t size() {
        return (_end + _data_cache.size() - _begin) % _data_cache.size(); 
    }

    void dropping() { // if there's no output_chan, the output will be dropped.
        T t;
        while(_input_chan.read(t)) {}
    }

    void replicating(size_t chan_id) {
        Channel<T> chan = _output_chans[chan_id];
        T t;
        for(;;) {
            std::unique_lock<std::mutex> mlock(_cache_mutex);
            _not_empty_cond.wait(mlock, [this, &chan_id]() { 
                    return _read_pos[chan_id] != _end || _caching_closed;});

            if (_caching_closed && _read_pos[chan_id] == _end) {
                break;
            }

            t = _data_cache[_read_pos[chan_id]];

            advance_pos(_read_pos[chan_id]);
            size_t min_pos = _read_pos[chan_id];
            for (auto pos : _read_pos) {
                if (front_of(pos, min_pos)) {
                    min_pos = pos;
                }
            }

            if (front_of(_begin, min_pos)) {
                _begin = min_pos;
                _not_full_cond.notify_one();
            }
            mlock.unlock();

            SCHECK(chan.write(std::move(t))) << "Channel closed.";
        }
        chan.close();
    }

    void caching() {
        T t;
        while(_input_chan.read(t)) {
            std::unique_lock<std::mutex> mlock(_cache_mutex);
            _not_full_cond.wait(mlock, [this]() { return !full();});
            _data_cache[_end] = std::move(t);
            advance_pos(_end);
            _not_empty_cond.notify_all();
        }
        std::unique_lock<std::mutex> mlock(_cache_mutex);
        _caching_closed = true;
        _not_empty_cond.notify_all();
    }

private:
    Channel<T> _input_chan;
    std::vector<Channel<T>> _output_chans;
    bool _inited = false;
    std::mutex _cache_mutex;
    std::vector<T> _data_cache;
    std::vector<std::thread> _rep_threads;
    std::thread _cache_thread;
    std::condition_variable _not_full_cond;
    std::condition_variable _not_empty_cond;
    size_t _data_cache_size = 8;
    size_t _begin = 0;
    size_t _end = 0;
    std::vector<size_t> _read_pos;
    bool _caching_closed = true;
};

template<>
class ChannelReplicator<void> { 
public:
    ChannelReplicator() {}

    ChannelReplicator(size_t) {}

    ChannelReplicator(size_t, size_t) {}

    ~ChannelReplicator() {}

    void initialize() {}

    void set_chan_capacity(size_t) {
        SLOG(FATAL) << "Not allowed";
    }

    void set_cache_size(size_t) {
        SLOG(FATAL) << "Not allowed";
    }

    size_t chan_capacity() {
        SLOG(FATAL) << "Not allowed";
        return 0;
    }

    size_t cache_size() {
        SLOG(FATAL) << "Not allowed";
        return 0;
    }

    void close() {}

    void join() {}

    void finalize() {}
};

} // namespace core
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_CHANNEL_REPLICATOR_H
