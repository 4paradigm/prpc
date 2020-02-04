#ifndef PREDICTOR_OBSERVABILITY_METRICS_HASH_H
#define PREDICTOR_OBSERVABILITY_METRICS_HASH_H

#include <cstddef>
#include <functional>

namespace pico {

namespace detail {

inline void hash_combine(std::size_t *) {}

template <typename T>
inline void hash_combine(std::size_t *seed, const T &value) {
    *seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (*seed << 6) + (*seed >> 2);
}

template <typename T, typename... Types>
inline void hash_combine(std::size_t *seed, const T &value,
        const Types &... args) {
    hash_combine(seed, value);
    hash_combine(seed, args...);
}

template <typename... Types>
inline std::size_t hash_value(const Types &... args) {
    std::size_t seed = 0;
    hash_combine(&seed, args...);
    return seed;
}

}  // namespace detail
}  // namespace pico

#endif // PREDICTOR_OBSERVABILITY_METRICS_HASH_H
