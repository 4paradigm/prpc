//
// Created by wangji on 2020/1/20.
//

#ifndef PREDICTOR_OBSERVABILITY_METRICS_DURATIONOBSERVER_H
#define PREDICTOR_OBSERVABILITY_METRICS_DURATIONOBSERVER_H

#include <chrono>

#include <prometheus/histogram.h>

#include "Metrics.h"

namespace paradigm4 {
namespace pico {

class DurationObserver {
public:
    DurationObserver(prometheus::Histogram& histogram);

    virtual ~DurationObserver();

private:
    std::chrono::steady_clock::time_point _start;
    prometheus::Histogram* _histogram;
};

} // namespace pico
} // namespace paradigm4

#endif //PREDICTOR_OBSERVABILITY_METRICS_DURATIONOBSERVER_H
