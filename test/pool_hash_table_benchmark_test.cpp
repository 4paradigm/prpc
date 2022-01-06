#include <cstdlib>
#include <cstdio>
#include <string>
#include <type_traits>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "common.h"
#include <sparsehash/dense_hash_map>
#include "PoolHashTable.h"

namespace paradigm4 {
namespace pico {
namespace core {

constexpr int N = 1 << 24;

struct feature_index_t {
    int64_t slot;
    int64_t sign;

    feature_index_t() : feature_index_t(0, 0) {}

    feature_index_t(int64_t sl, int64_t si) : slot(sl), sign(si) {}

    bool operator == (const feature_index_t& b) const {
        return slot == b.slot && sign == b.sign;
    }

    size_t hash() const noexcept {
        return static_cast<size_t>(slot * 107 + sign);
    }

    static feature_index_t& empty_key() {
        static feature_index_t empty({-1, -1});
        return empty;
    }

    static feature_index_t& deleted_key() {
        static feature_index_t deleted({-2, -2});
        return deleted;
    }
    PICO_SERIALIZATION(slot, sign);
};

struct LRValue {
    double weight = 0.0;
    double z = 0.0;
    double n = 1.0;
    double decayed_show = 0.0;

    LRValue() {}
    LRValue(feature_index_t key): weight(key.sign) {}

    PICO_SERIALIZATION(weight, z, n, decayed_show);
};

struct feature_index_hasher_t {
    size_t operator()(const feature_index_t& x) const noexcept {
        return static_cast<size_t>(x.slot * 107 + x.sign);
    }
};


class timer {
public: 
    timer(): start(std::chrono::high_resolution_clock::now()) {}

    void logging(std::string name, size_t count) {
        auto dur = std::chrono::high_resolution_clock::now() - start;
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count();
        SLOG(INFO) << name << ": " << ns / count << "ns";
        start = std::chrono::high_resolution_clock::now();
    }

    std::chrono::high_resolution_clock::time_point start;
};

int64_t randint(int64_t min, int64_t max) {
    static thread_local std::mt19937 gen(std::rand());
    std::uniform_int_distribution<int64_t> dis(min, max);
    return dis(gen);
}


TEST(pool_hash_map, random_find) {
    for (double load_factor = 0.5; load_factor < 1.01; load_factor += 0.1) {
        std::vector<feature_index_t> insert_keys;
        std::vector<feature_index_t> find_keys;
        for (int i = 0; i < load_factor * N; ++i) {
            feature_index_t key = {i, randint(0, N)};
            insert_keys.push_back(key);
            find_keys.push_back(key);
        }
        std::random_shuffle(insert_keys.begin(), insert_keys.end());
        std::random_shuffle(find_keys.begin(), find_keys.end());

        double sum1 = 0.0, sum2 = 0.0, sum3 = 0.0;
        if (load_factor < 1.01) {
            pool_hash_map<feature_index_t, LRValue, feature_index_hasher_t> table;
            table.max_load_factor(1.0);
            table.reserve(N + 1);
            SLOG(INFO) << "bucket count: " << table.bucket_count();
            timer time;
            for (feature_index_t key: insert_keys) {
                table[key] = key;
            }
            time.logging("pool_hash_table::insert", table.size());
            for (feature_index_t key: find_keys) {
                sum1 += table.find(key)->second.weight;
            }
            time.logging("pool_hash_table::find", table.size());
            SLOG(INFO) << "bucket count: " << table.bucket_count() << ' ' << table.size();
        }
        if (load_factor < 1.01) {
            pool_hash_map<feature_index_t, LRValue, feature_index_hasher_t> table;
            table.max_load_factor(1.0);
            table.reserve(N);
            SLOG(INFO) << "bucket count: " << table.bucket_count();
            timer time;
            for (feature_index_t key: insert_keys) {
                table[key] = key;
            }
            time.logging("pool_hash_table2::insert", table.size());
            for (feature_index_t key: find_keys) {
                sum2 += table.find(key)->second.weight;
            }
            time.logging("pool_hash_table2::find", table.size());
            SLOG(INFO) << "bucket count: " << table.bucket_count() << ' ' << table.size();
        }
        if (load_factor < 1.01) {
            google::dense_hash_map<feature_index_t, LRValue, feature_index_hasher_t> table;
            table.set_empty_key(feature_index_t::empty_key());
            table.max_load_factor(0.95);
            table.rehash(N / 2);
            SLOG(INFO) << "bucket count: " << table.bucket_count();

            timer time;
            for (feature_index_t key: insert_keys) {
                table[key] = key;
            }
            time.logging("google::dense_hash_table::insert", table.size());
            for (feature_index_t key: find_keys) {
                sum3 += table.find(key)->second.weight;
            }
            time.logging("google::dense_hash_table::find", table.size());
            SLOG(INFO) << "bucket count: " << table.bucket_count() << ' ' << table.size();
        }
        SLOG(INFO) << sum1 << ' ' << sum2 << ' ' << sum3;
        EXPECT_EQ(sum1, sum2);
        EXPECT_EQ(sum1, sum3);
    }
}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
