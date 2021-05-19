#ifndef PARADIGM4_PICO_COMMON_SUMAGGREGATOR_H
#define PARADIGM4_PICO_COMMON_SUMAGGREGATOR_H

#include "Aggregator.h"

namespace paradigm4 {
namespace pico {
namespace core {

template<class VAL>
class SumAggregator : public Aggregator<VAL, SumAggregator<VAL>> {
public:
    void init() {
        _value = VAL();
    }

    void serialize(core::BinaryArchive& ba) {
        ba << _value;
    }

    void deserialize(core::BinaryArchive& ba) {
        ba >> _value;
    }

    bool try_to_string(std::string& ret_str) {
        return pico_lexical_cast(_value, ret_str);
    }

    void merge_value(const VAL& val) {
        _value += val;
    }

    void merge_aggregator(const SumAggregator& agg) {
        merge_value(agg._value);
    }

private:
    VAL _value;
};

}
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_SUMAGGREGATOR_H
