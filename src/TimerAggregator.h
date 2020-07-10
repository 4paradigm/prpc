#ifndef PARADIGM4_PICO_CORE_TIMERAGGREGATOR_H
#define PARADIGM4_PICO_CORE_TIMERAGGREGATOR_H

#include "Aggregator.h"
#include "pico_lexical_cast.h"

namespace paradigm4 {
namespace pico {
namespace core {

template<class VAL>
class TimerAggregator : public Aggregator<VAL, TimerAggregator<VAL>> {
public:
    static_assert(std::is_arithmetic<VAL>::value, "support arithmetic type only.");

    void init() {
        _sum = VAL();
        _sum2 = VAL();
        _min = std::numeric_limits<VAL>::max();
        _max = std::numeric_limits<VAL>::min();
        _count = 0;
    }

    void serialize(core::BinaryArchive& ba) {
        ba << _sum << _sum2 << _min << _max << _count;
    }

    void deserialize(core::BinaryArchive& ba) {
        ba >> _sum >> _sum2 >> _min >> _max >> _count;
    }

    bool try_to_string(std::string& ret_str) {
        if (0 == _count) {
            ret_str = "N/A";
        } else {
            std::string total_str;
            std::string count_str;
            std::string min_str;
            std::string max_str;
            std::string mean_str;
            std::string var_str;
            if (pico_lexical_cast(_sum, total_str) &&
                    pico_lexical_cast(_count, count_str) &&
                    pico_lexical_cast(_min, min_str) &&
                    pico_lexical_cast(_max, max_str) &&
                    pico_lexical_cast(_sum / static_cast<double>(_count), mean_str) &&
                    pico_lexical_cast(_sum2 / static_cast<double>(_count) 
                        - _sum * _sum / static_cast<double>(_count) / static_cast<double>(_count), var_str)) {
                ret_str = format_string("TOTAL=%s COUNT=%s MEAN=%s VARIANCE=%s MIN=%s MAX=%s",
                        total_str.c_str(),
                        count_str.c_str(),
                        mean_str.c_str(),
                        var_str.c_str(),
                        min_str.c_str(),
                        max_str.c_str());
                return true;
            } else {
                return false;
            }
        }
        return true;
    }

    void merge_value(const VAL& val) {
        _sum += val;
        _sum2 += val * val;
        if (_min > val) {
            _min = val;
        }
        if (_max < val) {
            _max = val;
        }
        _count++;
    }

    void merge_aggregator(const TimerAggregator<VAL>& agg) {
        _sum += agg._sum;
        _sum2 += agg._sum2;
        if (_min > agg._min) {
            _min = agg._min;
        }
        if (_max < agg._max) {
            _max = agg._max;
        }
        _count += agg._count;
    }

private:
    VAL _sum;
    VAL _sum2;
    VAL _min;
    VAL _max;
    size_t _count;
};

}
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_TIMERAGGREGATOR_H
