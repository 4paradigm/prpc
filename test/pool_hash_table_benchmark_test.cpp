#include <cstdlib>
#include <cstdio>
#include <string>
#include <type_traits>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "common.h"
#include <sparsehash/dense_hash_map>
#include "PoolHashTable.h"
#include "ThreadGroup.h"

namespace paradigm4 {
namespace pico {
namespace core {

constexpr int N = 1 << 26, NT = 16;

struct feature_index_t {
    int64_t slot;
    int64_t sign;

    feature_index_t() : feature_index_t(0, 0) {}

    feature_index_t(int64_t sl, int64_t si) : slot(sl), sign(si) {}

    bool operator == (const feature_index_t& b) const {
        return slot == b.slot && sign == b.sign;
    }

    size_t hash()const noexcept {
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
    double pad1;
    double pad2;
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
        record = ns / count;
        start = std::chrono::high_resolution_clock::now();
    }

    std::chrono::high_resolution_clock::time_point start;
    int record = 0;
};

int64_t randint(int64_t min, int64_t max) {
    static thread_local std::mt19937 gen(std::rand());
    std::uniform_int_distribution<int64_t> dis(min, max);
    return dis(gen);
}


TEST(pool_hash_map, random_find) {
    std::vector<std::vector<int>> summary;
    std::vector<int> NS{N, N - 1};
    for (int n: NS) for (double load_factor = 0.1; load_factor < 1.01; load_factor += 0.1) {
        summary.emplace_back();
        SLOG(INFO) << load_factor;
        std::vector<feature_index_t> insert_keys;
        std::vector<feature_index_t> find_keys;
        for (int i = 0; i < load_factor * n; ++i) {
            feature_index_t key = {i, randint(0, std::numeric_limits<int64_t>::max())};
            insert_keys.push_back(key);
            find_keys.push_back(key);
        }
        std::random_shuffle(insert_keys.begin(), insert_keys.end());
        std::random_shuffle(find_keys.begin(), find_keys.end());
        std::random_shuffle(find_keys.begin(), find_keys.end());

        double sum1 = 0.0, sum2 = 0.0;
        if (load_factor < 1.01) {
            pool_hash_map<feature_index_t, LRValue, feature_index_hasher_t> table;
            SLOG(INFO) << sizeof(table);
            table.max_load_factor(1.0);
            table.reserve(n);
            
            SLOG(INFO) << "bucket count: " << table.bucket_count();
            timer time;
            for (feature_index_t key: insert_keys) {
                table[key] = key;
            }
            SLOG(INFO) << "bucket count: " << table.bucket_count() << ' ' << table.size();
            time.logging("pool_hash_table::insert", table.size());
            summary.back().push_back(time.record);

            for (feature_index_t key: find_keys) {
                sum1 += table.find(key)->second.weight;
            }
            time.logging("pool_hash_table::find", table.size());
            summary.back().push_back(time.record);
            
            std::vector<double> sums(NT);
            std::vector<std::thread> threads;
            for (size_t tid = 0; tid < NT; ++tid) {
                threads.emplace_back([&](int tid){
                    double sum = 0.0;
                    size_t n = find_keys.size() / NT;
                    for (size_t i = tid * n; i < tid * n + n; i += 1) {
                        sum += table.find(find_keys[i])->second.weight;
                    }
                    sums[tid] = sum;
                }, tid);
            }
            for (size_t tid = 0; tid < NT; ++tid) {
                threads[tid].join();
                sum1 += sums[tid];
            }
            time.logging("pool_hash_table::find mt", table.size() / NT);
            summary.back().push_back(time.record);
        }

        if (load_factor < 1.01) {
            google::dense_hash_map<feature_index_t, LRValue, feature_index_hasher_t> table;
            SLOG(INFO) << sizeof(table);
            table.set_empty_key(feature_index_t::empty_key());
            table.max_load_factor(0.95);
            table.rehash((n + 1) / 2);
            SLOG(INFO) << "bucket count: " << table.bucket_count();
            timer time;
            for (feature_index_t key: insert_keys) {
                table[key] = key;
            }
            SLOG(INFO) << "bucket count: " << table.bucket_count() << ' ' << table.size();
            time.logging("google::dense_hash_table::insert", table.size());
            summary.back().push_back(time.record);

            for (feature_index_t key: find_keys) {
                sum2 += table.find(key)->second.weight;
            }
            time.logging("google::dense_hash_table::find", table.size());
            summary.back().push_back(time.record);
            
            std::vector<double> sums(NT);
            std::vector<std::thread> threads;
            for (size_t tid = 0; tid < NT; ++tid) {
                threads.emplace_back([&](int tid){
                    double sum = 0.0;
                    size_t n = find_keys.size() / NT;
                    for (size_t i = tid * n; i < tid * n + n; i += 1) {
                        sum += table.find(find_keys[i])->second.weight;
                    }
                    sums[tid] = sum;
                }, tid);
            }
            for (size_t tid = 0; tid < NT; ++tid) {
                threads[tid].join();
                sum2 += sums[tid];
            }
            time.logging("google::dense_hash_table::find mt", table.size() / NT);
            summary.back().push_back(time.record);
        }

        SLOG(INFO) << sum1 << ' ' << sum2;
        EXPECT_EQ(sum1, sum2);
    }
    std::string report = "";
    for (auto& line: summary) {
        report += '\n';
        for (auto& record: line) {
            report += std::to_string(record) + '\t';
        }
    }
    SLOG(INFO) << report;
}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
