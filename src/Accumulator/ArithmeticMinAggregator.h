#ifndef PARADIGM4_PICO_CORE_ARITHMETICMINAGGREGATOR_H
#define PARADIGM4_PICO_CORE_ARITHMETICMINAGGREGATOR_H

#include "Aggregator.h"


namespace paradigm4 {
namespace pico {
namespace core {

template<class VAL>
class ArithmeticMinAggregator : public Aggregator<VAL, ArithmeticMinAggregator<VAL>> {
public:
    static_assert(std::is_arithmetic<VAL>::value, "support arithmetic type only.");
    
    void init() {
        _value = std::numeric_limits<VAL>::max();
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
        if (val < _value) {
            _value = val;
        }
    }

    void merge_aggregator(const ArithmeticMinAggregator<VAL>& agg) {
        merge_value(agg._value);
    }

private:
    VAL _value;
};

}
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_ARITHMETICMINAGGREGATOR_H
