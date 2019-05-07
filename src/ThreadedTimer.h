#ifndef PARADIGM4_PICO_COMMON_THREADEDTIMER_H
#define PARADIGM4_PICO_COMMON_THREADEDTIMER_H

#include "TimerAggregator.h"
#include <unordered_map>
#include <chrono>

#include "pico_framework_configure.h"

namespace paradigm4 {
namespace pico {

class AggData {
public:

    static AggData& singleton() {
        static thread_local AggData ins;
        return ins;
    }

    void flush() {
        for (auto& tm : _data) {
            std::string ret;
            if (tm.second.try_to_string(ret)) {
                SLOG(INFO) << tm.first << " : " << ret;
            } else {
                SLOG(INFO) << "failed to get " << tm.first;
            }
        }
        _data.clear();
    }

    void write(const std::string& name, double dur) {
        _data[name].merge_value(dur);
    }

    void write_pending(const std::string& name, double dur) {
        _pending[name] += dur;
    }

    void flush_pending() {
        for (auto& pr : _pending) {
            write(pr.first, pr.second);
        }
        _pending.clear();
    }

    ~AggData() {
        flush();
    }

public:
    std::unordered_map<std::string, TimerAggregator<double>> _data;
    std::unordered_map<std::string, double> _pending;
};

class timer_guard {
public:
    timer_guard(AggData* agg, const std::string& name) {
        if (!pico_framework_configure().performance.is_evaluate_performance) {
            return;
        }
        _agg = agg;
        _name = name;
        _start = std::chrono::steady_clock::now();
    }

    timer_guard(const std::string& name) {
        if (!pico_framework_configure().performance.is_evaluate_performance) {
            return;
        }
        _agg = &AggData::singleton();
        _name = name;
        _start = std::chrono::steady_clock::now();
    }

    ~timer_guard() {
        if (!pico_framework_configure().performance.is_evaluate_performance) {
            return;
        }
        auto dur = std::chrono::steady_clock::now() - _start;
        double ms = std::chrono::duration_cast<
                    std::chrono::duration<double, std::ratio<1, 1000>>>(dur)
                    .count();
        if (_agg) {
            _agg->write(_name, ms);
        } else {
            SLOG(INFO) << _name << " : " << ms;
        }
    }

    std::chrono::steady_clock::time_point _start;
    AggData* _agg;
    std::string _name;
};

} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_THREADEDTIMER_H
