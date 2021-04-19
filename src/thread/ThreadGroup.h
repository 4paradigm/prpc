#ifndef PARADIGM4_PICO_CORE_THREAD_GROUP_H
#define PARADIGM4_PICO_CORE_THREAD_GROUP_H

#include <thread>

#include "Channel.h"
#include "AsyncReturn.h"
#include "VirtualObject.h"

namespace paradigm4 {
namespace pico {
namespace core {

class ThreadGroup : public NoncopyableObject {
public:
    ThreadGroup() : ThreadGroup(std::thread::hardware_concurrency()) {
    }

    explicit ThreadGroup(size_t thread_num) {
        _threads.resize(thread_num);
        for (size_t thread_id = 0; thread_id < thread_num; ++thread_id) {
            _threads[thread_id] = std::thread(&ThreadGroup::run, this, thread_id);
        }
    }

    ~ThreadGroup() {
        if (!_exec_chan.closed()) {
            _exec_chan.close();
        }
        join();
    }

    bool async_exec(AsyncReturn& ret, std::function<void(int)> func) {
        return _exec_chan.write({ret, func});
    }

    AsyncReturn async_exec(std::function<void(int)> func) {
        AsyncReturn ret;
        SCHECK(_exec_chan.write({ret, func}));
        return ret;
    }

    size_t size() const {
        return _threads.size();
    }

    void resize(size_t thread_num) {
        if (!_exec_chan.closed()) {
            _exec_chan.close();
        }
        join();

        _exec_chan.open();
        _threads.resize(thread_num);
        for (size_t id = 0; id < thread_num; ++id) {
            _threads[id] = std::thread(&ThreadGroup::run, this, id);
        }
    }

    void terminate() {
        _exec_chan.close();
    }

    void join() {
        for (auto& thrd : _threads) {
            if (thrd.joinable()) {
                thrd.join();
            }
        }
    }

private:
    void run(int thread_id) {
        std::pair<AsyncReturn, std::function<void(int)>> exec_pair;
        while (_exec_chan.read(exec_pair)) {
            exec_pair.second(thread_id);
            exec_pair.first.notify();
        }
    }

private:
    std::vector<std::thread> _threads;
    Channel<std::pair<AsyncReturn, std::function<void(int)>>> _exec_chan;
};

}
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_ARITHMETICMAXAGGREGATOR_H
