#ifndef PARADIGM4_PICO_COMMON_AGGREGATOR_H
#define PARADIGM4_PICO_COMMON_AGGREGATOR_H

#include "AggregatorFactory.h"
#include "Archive.h"
#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

class AggregatorBase : public core::Object {
public:
    virtual void init() = 0;

    virtual void merge_aggregator(AggregatorBase*) = 0;

    virtual void serialize(BinaryArchive&) = 0;

    virtual void deserialize(BinaryArchive&) = 0;

    virtual bool try_to_string(std::string&) = 0;
};

/*!
 * \brief Aggregator used to store some statistics variable.
 *  all Aggregators inherit from this base class need to implement merge_value()
 *  function(add some delta to current aggregator).
 *  aggregator used as base variable in accumulator.
 */
template<class VAL, class AGG_SELF>
class Aggregator : public AggregatorBase, AggregatorRegisterAgent<AGG_SELF> {
public:
    typedef VAL value_type;

    typedef AGG_SELF self_type;

    virtual void merge_value(const value_type& val) = 0;

    virtual void merge_value(value_type&& val) { // user can override this if needed
        return merge_value(val);
    }

    virtual void merge_aggregator(const self_type& agg) = 0;

    virtual void merge_aggregator(self_type&& agg) { // user can override this if needed
        return merge_aggregator(agg);
    }

    virtual void merge_aggregator(AggregatorBase* agg_ptr) {
        SCHECK(agg_ptr) << "Aggregator ptr is null";
        bool is_move = true;
        is_move ? merge_aggregator(std::move(*static_cast<self_type*>(agg_ptr))) 
            : merge_aggregator(*static_cast<self_type*>(agg_ptr));
    }
};

}
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_AGGREGATOR_H
