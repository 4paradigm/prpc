//
// Created by wangji on 2020/1/20.
//

#include "DurationObserver.h"

namespace paradigm4 {
namespace pico {

DurationObserver::DurationObserver(prometheus::Histogram& histogram) {
    _histogram = &histogram;
    _start = std::chrono::steady_clock::now();
}

DurationObserver::~DurationObserver() {
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start).count();
    _histogram->Observe(dur);
}

} // namespace pico
} // namespace paradigm4
