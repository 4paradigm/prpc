#ifndef PARADIGM4_PICO_CORE_MACRO_H
#define PARADIGM4_PICO_CORE_MACRO_H

#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/facilities/overload.hpp>

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif // likely

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif // unlikely

#ifndef PICO_DEPRECATED
#define PICO_DEPRECATED(msg) [[deprecated(msg)]]
#endif // PICO_DEPRECATED


#endif // PARADIGM4_PICO_CORE_MACRO_H
