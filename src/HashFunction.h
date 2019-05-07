#ifndef PARADIGM4_PICO_COMMON_HASH_FUNCTION_H
#define PARADIGM4_PICO_COMMON_HASH_FUNCTION_H

#include <cstdint>
#include "VirtualObject.h"

namespace paradigm4 {
namespace pico {

class HashFunction : public VirtualObject {
public:
    HashFunction() = delete;
    static int32_t jump_consistent_hash(uint64_t key, int32_t num_buckets) {
        int64_t b = -1;
        int64_t j = 0;

        while (j < num_buckets) {
            b = j;
            key = key * 2862933555777941757ULL + 1;
            j = (b + 1) * (double(1LL << 31) / double((key >> 33) + 1));
        }

        return b;
    }

    static uint64_t murmur_hash(uint64_t x) {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccd;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53;
        x ^= x >> 33;
        return x;
    }

    static uint64_t murmur_hash_half(uint64_t x) {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccd;
        x ^= x >> 33;
        return x;
    }

    static uint64_t bit_reverse(uint64_t i) {
        i = (i & 0x5555555555555555ULL) << 1 | ((i >> 1) & 0x5555555555555555ULL);
        i = (i & 0x3333333333333333ULL) << 2 | ((i >> 2) & 0x3333333333333333ULL);
        i = (i & 0x0f0f0f0f0f0f0f0fULL) << 4 | ((i >> 4) & 0x0f0f0f0f0f0f0f0fULL);
        i = (i & 0x00ff00ff00ff00ffULL) << 8 | ((i >> 8) & 0x00ff00ff00ff00ffULL);
        i = (i << 48) | ((i & 0xffff0000ULL) << 16) |
            ((i >> 16) & 0xffff0000ULL) | (i >> 48);
        return i;
    }
};

} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_HASH_FUNCTION_H
