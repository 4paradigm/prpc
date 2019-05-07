#ifndef PARADIGM4_PICO_COMMON_THREAD_GROUP_H
#define PARADIGM4_PICO_COMMON_THREAD_GROUP_H

#include <thread>
#include <functional>

#include "VirtualObject.h"
#include "Channel.h"
#include "pico_log.h"
#include "AsyncReturn.h"
#include "pico_framework_configure.h"

namespace paradigm4 {
namespace pico {

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

inline ThreadGroup& pico_thread_group() {
    static ThreadGroup tg(pico_framework_configure().global_concurrency);
    return tg;
}

inline size_t pico_concurrency() {
    return pico_thread_group().size();
}

template<typename FUNC>
void pico_parallel_run(FUNC func, ThreadGroup& tg = pico_thread_group()) {
    std::vector<AsyncReturn> rets(tg.size());

    for (size_t i = 0; i < rets.size(); ++i) {
        SCHECK(tg.async_exec(rets[i], [&func, i](int thrd_id){ func(thrd_id, i);}));
    }

    for (auto& ret : rets) {
        ret.wait();
    }
}

template<typename FUNC>
void pico_parallel_run_range(size_t n, FUNC func, ThreadGroup& tg = pico_thread_group()) {
    std::vector<AsyncReturn> rets(tg.size());

    size_t concurrency = rets.size();
    for (size_t i = 0; i < rets.size(); ++i) {
        size_t start = i * n / concurrency;
        size_t end = std::min((i + 1) * n / concurrency, n);
        SCHECK(tg.async_exec(rets[i], [&func, i, start, end](int thrd_id) {
                        func(thrd_id, i, start, end);}));
    }

    for (auto& ret : rets) {
        ret.wait();
    }
}

template<typename FUNC>
void pico_parallel_run_dynamic(size_t n, FUNC func, ThreadGroup& tg = pico_thread_group()) {
    std::vector<AsyncReturn> rets(n);

    for (size_t i = 0; i < n; ++i) {
        SCHECK(tg.async_exec(rets[i], [&func, i](int thrd_id) {
                    func(thrd_id, i);}));
    }

    for (auto& ret : rets) {
        ret.wait();
    }
}

class GlobalParalleling {
};

template<typename FUNC>
void pico_global_parallel_run(FUNC func, ThreadGroup& tg = pico_thread_group()) {
    size_t id = AtomicID<GlobalParalleling, size_t>::gen();
    std::vector<AsyncReturn> rets(tg.size());

    for (size_t i = 0; i < rets.size(); ++i) {
        SCHECK(tg.async_exec(rets[i], [&func, i](int thrd_id){ func(thrd_id, i);}));
    }

    for (auto& ret : rets) {
        ret.wait();
    }
    pico_barrier(format_string("%s_%d", PICO_FILE_LINENUM, id));
}

template<typename FUNC>
void pico_global_parallel_run_range(size_t n, FUNC func, ThreadGroup& tg = pico_thread_group()) {
    size_t id = AtomicID<GlobalParalleling, size_t>::gen();
    std::vector<AsyncReturn> rets(tg.size());

    size_t local_begin = pico_comm_rank() * n / pico_comm_size();
    size_t local_end = std::min((pico_comm_rank() + 1 ) * n / pico_comm_size(), n);
    size_t concurrency = rets.size();
    size_t local_n = local_end - local_begin;
    for (size_t i = 0; i < rets.size(); ++i) {
        size_t start = local_begin + i * local_n / concurrency;
        size_t end = std::min(local_begin + (i + 1) * local_n / concurrency, local_end);
        SCHECK(tg.async_exec(rets[i], [&func, i, start, end](int thrd_id) {
                        func(thrd_id, i, start, end);}));
    }

    for (auto& ret : rets) {
        ret.wait();
    }
    pico_barrier(format_string("%s_%d", PICO_FILE_LINENUM, id));
}

} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_THREAD_GROUP_H
