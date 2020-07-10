#ifndef PARADIGM4_PICO_CORE_AVGAGGREGATOR_H
#define PARADIGM4_PICO_CORE_AVGAGGREGATOR_H

#include "Aggregator.h"

namespace paradigm4 {
namespace pico {
namespace core {

template <class VAL, class KEY = void>
class AvgAggregator : public Aggregator<std::pair<KEY, VAL>, AvgAggregator<VAL, KEY>> {
public:
    void init() {
        _value.clear();
    }

    void serialize(core::BinaryArchive& ba) {
        ba << _value;
    }

    void deserialize(core::BinaryArchive& ba) {
        ba >> _value;
    }

    bool try_to_string(std::string& ret_str) {
        PicoJsonNode node;
        for (auto& pr : _value) {
            std::string str;
            pico_lexical_cast(pr.first, str);
            node.add(str, pr.second.first / pr.second.second);
        }
        SCHECK(node.save(ret_str, false));
        return true;
    }

    void merge_value(const std::pair<KEY, VAL>& val) {
        auto& t = _value[val.first];
        t.first += val.second;
        ++t.second;
    }

    void merge_aggregator(const AvgAggregator<VAL, KEY>& agg) {
        for (auto& pr : agg._value) {
            auto& h = _value[pr.first];
            h.first += pr.second.first;
            h.second += pr.second.second;
        }
    }

private:
    std::unordered_map<KEY, std::pair<VAL, size_t>> _value;
};

template <class VAL>
class AvgAggregator<VAL, void> : public Aggregator<VAL, AvgAggregator<VAL, void>> {
public:
    void init() {
        _value = VAL();
        _count = 0;
    }

    void serialize(core::BinaryArchive& ba) {
        ba << _value << _count;
    }

    void deserialize(core::BinaryArchive& ba) {
        ba >> _value >> _count;
    }

    bool try_to_string(std::string& ret_str) {
        if (0 == _count) {
            ret_str = "N/A";
        } else {
            return pico_lexical_cast(_value / static_cast<double>(_count), ret_str);
        }
        return true;
    }

    double average() {
        return static_cast<double>(_value / static_cast<double>(_count));
    }

    void merge_value(const VAL& val) {
        merge_value_internal(val, 1);
    }

    void merge_aggregator(const AvgAggregator<VAL>& agg) {
        merge_value_internal(agg._value, agg._count);
    }

private:
    void merge_value_internal(const VAL& val, size_t cnt) {
        _value += val;
        _count += cnt;
    }

private:
    VAL _value;
    size_t _count;
};

}
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_AVGAGGREGATOR_H
