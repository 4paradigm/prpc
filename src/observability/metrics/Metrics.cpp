//
// Created by wangji on 2020/1/19.
//

#include "Metrics.h"
#include "hash_combine.h"
#include "pico_log.h"

#include <prometheus/counter.h>
#include <prometheus/detail/utils.h>

namespace paradigm4 {
namespace pico {

const std::string Metrics::kLabelNameServiceName = "service_name";
const std::string Metrics::kLabelNameInstanceName = "instance_name";

thread_local std::unordered_map<size_t, prometheus::Counter*> Metrics::_counter_tls;
thread_local std::unordered_map<size_t, prometheus::Gauge*> Metrics::_gauge_tls;
thread_local std::unordered_map<size_t, prometheus::Histogram*> Metrics::_histogram_tls;

Metrics::Metrics() {}

void Metrics::initialize(const std::string& exposer_ip,
                         int32_t exposer_port,
                         const std::string& exposer_url,
                         const std::string& service_name,
                         const std::string& instance_name,
                         bool enable) {
    std::lock_guard<std::mutex> lock(_lock);

    if (_initialized) {
        return;
    }

    _exposer_ip = exposer_ip;
    _exposer_port = exposer_port;
    _exposer_url = exposer_url;
    _service_name = service_name;
    _instance_name = instance_name;
    _enabled = enable;

    _registry = std::make_shared<prometheus::Registry>();
    _common_labels.emplace(Metrics::kLabelNameServiceName, _service_name);
    _common_labels.emplace(Metrics::kLabelNameInstanceName, _instance_name);

    if (_enabled) {
        _exposer = std::make_unique<prometheus::Exposer>(_exposer_ip + ":" + std::to_string(_exposer_port), _exposer_url, 1);
        _exposer->RegisterCollectable(_registry);
        SLOG(INFO) << "Metrics enabled, binding port: " << _exposer_port << ", exposer url: '" << _exposer_url << "'";
    } else {
        SLOG(INFO) << "Metrics disabled.";
    }

    _initialized = true;
}

void Metrics::finalize() {
}

prometheus::Family<prometheus::Counter>* Metrics::build_counter_family(const std::string& name,
                                                                       const std::string& help,
                                                                       const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(_lock);
    auto it = _counter_families.find(name);
    if (it != _counter_families.end()) {
        return it->second;
    }

    std::map<std::string, std::string> real_labels = labels;
    real_labels.insert(_common_labels.begin(), _common_labels.end());

    auto* ptr = &(prometheus::BuildCounter().Name(name).Help(help).Labels(real_labels).Register(*_registry));
    auto res = _counter_families.emplace(name, ptr);
    if (res.second) {
        return res.first->second;
    } else {
        return nullptr;
    }
}

prometheus::Family<prometheus::Gauge>* Metrics::build_gauge_family(const std::string& name,
                                                                   const std::string& help,
                                                                   const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(_lock);
    auto it = _gauge_families.find(name);
    if (it != _gauge_families.end()) {
        return it->second;
    }

    std::map<std::string, std::string> real_labels = labels;
    real_labels.insert(_common_labels.begin(), _common_labels.end());

    auto* ptr = &(prometheus::BuildGauge().Name(name).Help(help).Labels(real_labels).Register(*_registry));
    auto res = _gauge_families.emplace(name, ptr);
    if (res.second) {
        return res.first->second;
    } else {
        return nullptr;
    }
}

prometheus::Family<prometheus::Histogram>* Metrics::build_histogram_family(const std::string& name,
                                                                           const std::string& help,
                                                                           const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(_lock);
    auto it = _histogram_families.find(name);
    if (it != _histogram_families.end()) {
        return it->second;
    }

    std::map<std::string, std::string> real_labels = labels;
    real_labels.insert(_common_labels.begin(), _common_labels.end());

    auto* ptr = &(prometheus::BuildHistogram().Name(name).Help(help).Labels(real_labels).Register(*_registry));
    auto res = _histogram_families.emplace(name, ptr);
    if (res.second) {
        return res.first->second;
    } else {
        return nullptr;
    }
}

size_t Metrics::hash_metrics(const std::string& family_name, const std::map<std::string, std::string>& labels) {
    size_t hash = std::hash<std::string>{}(family_name);
    detail::hash_combine(&hash, prometheus::detail::hash_labels(labels));
    return hash;
}

prometheus::Counter& Metrics::get_counter(const std::string& family_name,
                                          const std::string& help,
                                          const std::map<std::string, std::string>& labels) {
    size_t find_key = hash_metrics(family_name, labels);
    auto it = _counter_tls.find(find_key);
    if (it == _counter_tls.end()) {
        prometheus::Counter* ptr =
                &(build_counter_family(family_name, help, {})->Add(labels));
        _counter_tls[find_key] = ptr;
        return *ptr;
    } else {
        return *(it->second);
    }
}

prometheus::Gauge& Metrics::get_gauge(const std::string& family_name,
                                      const std::string& help,
                                      const std::map<std::string, std::string>& labels) {
    size_t find_key = hash_metrics(family_name, labels);
    auto it = _gauge_tls.find(find_key);
    if (it == _gauge_tls.end()) {
        prometheus::Gauge* ptr =
                &(build_gauge_family(family_name, help, {})->Add(labels));
        _gauge_tls[find_key] = ptr;
        return *ptr;
    } else {
        return *(it->second);
    }
}

prometheus::Histogram& Metrics::get_histogram(const std::string& family_name,
                                              const std::string& help,
                                              const std::map<std::string, std::string>& labels,
                                              const std::vector<double>& boundaries) {
    size_t find_key = hash_metrics(family_name, labels);
    auto it = _histogram_tls.find(find_key);
    if (it == _histogram_tls.end()) {
        prometheus::Histogram* ptr =
                &(build_histogram_family(family_name, help, {})->Add(labels, boundaries));
        _histogram_tls[find_key] = ptr;
        return *ptr;
    } else {
        return *(it->second);
    }
}

} // namespace pico
} // namespace paradigm4
