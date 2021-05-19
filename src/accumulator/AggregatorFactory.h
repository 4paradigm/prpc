#ifndef PARADIGM4_PICO_CORE_AGGREGATOR_FACTORY_H
#define PARADIGM4_PICO_CORE_AGGREGATOR_FACTORY_H

#include <memory>
#include "Factory.h"

namespace paradigm4 {
namespace pico {
namespace core {

/*!
 * \brief usage: AggregatorFactory::singleton() == string to function map
 */
class AggregatorFactory : public core::Factory<> {
public:
    static AggregatorFactory& singleton() {
        static AggregatorFactory factory;
        return factory;
    }

private:
    AggregatorFactory() = default;
};

/*!
 * \brief usage:
 *  AggregatorRegisterAgent<AGG>: register AGG into factory
 */
template<class AGG>
class AggregatorRegisterAgent {
public:
    AggregatorRegisterAgent() {
        (void)dummy;
    }
    static bool dummy;
};

template<class AGG>
bool AggregatorRegisterAgent<AGG>::dummy = AggregatorFactory::singleton().register_producer<AGG>();

}
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_AGGREGATOR_FACTORY_H
