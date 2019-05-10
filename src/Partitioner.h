#ifndef PARADIGM4_PICO_COMMON_PARTITIONER_H
#define PARADIGM4_PICO_COMMON_PARTITIONER_H

#include <string>
#include <utility>
#include <functional>
#include <vector>
#include "defs.h"
#include "MurmurHash3.h"
#include "HashFunction.h"
#include "macro.h"
namespace paradigm4 {
namespace pico {
namespace core {
PICO_DEFINE_MEMBER_FUNC_CHECKER(hash);

template<typename T, typename Enable=void>
struct PicoHash;

template <typename T>
struct PicoHash<T, std::enable_if_t<pico_has_member_func_hash<T>::value>>{
    size_t operator()(const T& key) const noexcept {
        return key.hash();
    }
};

template <typename T>
struct PicoHash<T, std::enable_if_t<!pico_has_member_func_hash<T>::value>> :public std::hash<T>{

};

template<class KEY, class HASH = PicoHash<KEY>>
class Partitioner {
public:
    typedef KEY key_type;
    typedef HASH hash_type;

    size_t hash(const KEY& key) const {
        return _hasher(key);
    }

    size_t operator()(const KEY& key) const noexcept {
        return hash(key);
    }

    int32_t partition(const KEY& key, int32_t num_buckets) const {
        return HashFunction::jump_consistent_hash(this->hash(key), num_buckets);
    }

private:
    HASH _hasher;
};

template<class KEY>
class DefaultPartitioner : public Partitioner<KEY> {
};

template<>
class DefaultPartitioner<uint64_t> : public Partitioner<uint64_t> {
public:
    size_t hash(const uint64_t& key) const {
        return static_cast<size_t>(HashFunction::murmur_hash(key));
    }
};

template<>
class DefaultPartitioner<int64_t> : public Partitioner<int64_t> {
public:
    size_t hash(const int64_t& key) const {
        return static_cast<size_t>(HashFunction::murmur_hash(static_cast<uint64_t>(key)));
    }
};

template<>
class DefaultPartitioner<std::string> : public Partitioner<std::string> {
public:
    size_t hash(const std::string& key) const {
        std::pair<uint64_t, uint64_t> hash_code;
#if __x86_64__
        murmur_hash3_x64_128(key.c_str(), key.size(), MURMURHASH_SEED, &hash_code);
#else
        murmur_hash3_x86_128(key.c_str(), key.size(), MURMURHASH_SEED, &hash_code);
#endif
        return static_cast<size_t>(hash_code.first);
    }
};

} // namespace core
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_PARTITIONER_H
