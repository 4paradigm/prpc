#ifndef PARADIGM4_PICO_CORE_RANDOM_H
#define PARADIGM4_PICO_CORE_RANDOM_H

#include <stdint.h>

namespace paradigm4 {
namespace pico {
namespace core {

struct XorShift64S {
    uint64_t seed = 0;

    XorShift64S(uint64_t seed): seed(seed) {}

    uint64_t operator()() {
        seed ^= seed >> 12;
        seed ^= seed << 25;
        seed ^= seed >> 27;
        return seed * 0x2545F4914F6CDD1D;
    }
};

}
}
}

#endif
