//
// Created by wangji on 2020/1/19.
//

#ifndef PREDICTOR_OBSERVABILITY_METRICS_METRICS_H
#define PREDICTOR_OBSERVABILITY_METRICS_METRICS_H

#include <unordered_map>
#include <utility>
#include <map>
#include <memory>
#include <string>
#include <mutex>
#include <vector>

#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/family.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

namespace paradigm4 {
namespace pico {

class Metrics {
public:
    static const std::string kLabelNameServiceName;
    static const std::string kLabelNameInstanceName;

    virtual ~Metrics() {}

    static Metrics& Singleton() {
        static Metrics metrics;
        return metrics;
    }

    inline static std::vector<double> create_linear_bucket(double start, double end,
                                                           double step) {
        std::vector<double> bucket_boundaries{};
        for (auto i = start; i < end; i += step) {
            bucket_boundaries.push_back(i);
        }
        return bucket_boundaries;
    }

    inline static std::vector<double> create_geometric_bucket(double start, double end,
                                                              double ratio) {
        std::vector<double> bucket_boundaries{};
        for (auto i = start; i < end; i *= ratio) {
            bucket_boundaries.push_back(i);
        }
        return bucket_boundaries;
    }

    inline static std::vector<double> create_general_duration_bucket() {
        std::vector<double> bucket_boundaries{0.0f};
        std::vector<double> v1(100, 1.0f);
        std::vector<double> v2(90, 10.0f);
        std::vector<double> v3(40, 100.0f);
        // 0 - 100, step 1
        for (auto d : v1) {
            bucket_boundaries.emplace_back(bucket_boundaries.back() + d);
        }
        // 100 - 1000, step 10
        for (auto d : v2) {
            bucket_boundaries.emplace_back(bucket_boundaries.back() + d);
        }
        // 1000 - 5000, step 100
        for (auto d : v3) {
            bucket_boundaries.emplace_back(bucket_boundaries.back() + d);
        }
        return bucket_boundaries;
    }

    // ?????? exposer ????????????????????????????????? register???
    void initialize(const std::string& exposer_ip,
                    int32_t exposer_port,
                    const std::string& exposer_url,
                    const std::string& service_name,
                    const std::string& instance_name,
                    bool enable = true);

    bool enabled() const {
        return _enabled;
    }

    void finalize();

    prometheus::Family<prometheus::Counter>* build_counter_family(const std::string& name,
                                                                  const std::string& help,
                                                                  const std::map<std::string, std::string>& labels);

    prometheus::Family<prometheus::Gauge>* build_gauge_family(const std::string& name,
                                                              const std::string& help,
                                                              const std::map<std::string, std::string>& labels);

    prometheus::Family<prometheus::Histogram>* build_histogram_family(const std::string& name,
                                                                      const std::string& help,
                                                                      const std::map<std::string, std::string>& labels);

    // ?????? thread_local ??? Counter ??????
    prometheus::Counter& get_counter(const std::string& family_name,
                                     const std::string& help,
                                     const std::map<std::string, std::string>& labels);

    // ?????? thread_local ??? Gauge ??????
    prometheus::Gauge& get_gauge(const std::string& family_name,
                                 const std::string& help,
                                 const std::map<std::string, std::string>& labels);

    // ?????? thread_local ??? Histogram ??????
    prometheus::Histogram& get_histogram(const std::string& family_name,
                                         const std::string& help,
                                         const std::map<std::string, std::string>& labels,
                                         const std::vector<double>& boundaries);

private:
    Metrics();

    std::unique_ptr<prometheus::Exposer> _exposer;
    std::shared_ptr<prometheus::Registry> _registry;

    std::unordered_map<std::string, prometheus::Family<prometheus::Counter>*> _counter_families;  // name -> counter_family
    std::unordered_map<std::string, prometheus::Family<prometheus::Gauge>*> _gauge_families;  // name -> gauge_family
    std::unordered_map<std::string, prometheus::Family<prometheus::Histogram>*> _histogram_families;  // name -> gauge_family

    std::map<std::string, std::string> _common_labels;

    std::string _exposer_ip;
    int32_t _exposer_port;
    std::string _exposer_url;
    std::string _service_name;
    std::string _instance_name;

    static thread_local std::unordered_map<size_t, prometheus::Counter*> _counter_tls;  // hash -> counter
    static thread_local std::unordered_map<size_t, prometheus::Gauge*> _gauge_tls;  // hash -> gauge
    static thread_local std::unordered_map<size_t, prometheus::Histogram*> _histogram_tls;  // hash -> histogram

    bool _enabled = false;
    bool _initialized = false;
    std::mutex _lock;

    static size_t hash_metrics(const std::string& family_name, const std::map<std::string, std::string>& labels);
};

static inline prometheus::Counter& metrics_counter(const std::string& family_name,
                                                   const std::string& help,
                                                   const std::map<std::string, std::string>& labels) {
    return Metrics::Singleton().get_counter(family_name, help, labels);
}

static inline prometheus::Gauge& metrics_gauge(const std::string& family_name,
                                               const std::string& help,
                                               const std::map<std::string, std::string>& labels) {
    return Metrics::Singleton().get_gauge(family_name, help, labels);
}

static inline prometheus::Histogram& metrics_histogram(const std::string& family_name,
                                                       const std::string& help,
                                                       const std::map<std::string, std::string>& labels,
                                                       const std::vector<double>& boundaries) {
    return Metrics::Singleton().get_histogram(family_name, help, labels, boundaries);
}

static inline void metrics_initialize(const std::string& exposer_ip,
                                      int32_t exposer_port,
                                      const std::string& exposer_url,
                                      const std::string& service_name,
                                      const std::string& instance_name,
                                      bool enable = true) {
    Metrics::Singleton().initialize(exposer_ip, exposer_port, exposer_url, service_name, instance_name, enable);
}

static inline void metrics_finalize() {
    Metrics::Singleton().finalize();
}

}  // namespace pico
}  // namespace paradigm4

#endif //PREDICTOR_OBSERVABILITY_METRICS_METRICS_H
