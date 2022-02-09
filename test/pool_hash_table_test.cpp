#include <cstdlib>
#include <cstdio>
#include <string>
#include <type_traits>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "common.h"
#include "PoolHashTable.h"
#include <sparsehash/dense_hash_map>

namespace paradigm4 {
namespace pico {
namespace core {

int g_count = 0;

struct test_string {
    test_string() {
        ++g_count;
    }
    test_string(const char* other): inner(other) {
        ++g_count;
    }
    test_string(std::string other): inner(other) {
        ++g_count;
    }
    test_string(const test_string& other): inner(other.inner) {
        ++g_count;
    }
    ~test_string() {
        --g_count;
    }
    bool operator==(const test_string& other)const {
        return inner == other.inner;
    }
    bool operator!=(const test_string& other)const {
        return inner != other.inner;
    }
    std::string inner;
};

struct test_string_hash {
    size_t operator()(const test_string& a)const {
        return std::hash<std::string>()(a.inner);
    }
};

TEST(pool_hash_map, erase_ok) {
    pool_hash_map<std::string, int> ht;
    ht.rehash(4);
    ht.debug();
    ht["a"] = 10;
    ht["b"] = 20;
    ht["c"] = 30;
    ht["d"] = 40;
    ht.debug();
    EXPECT_EQ(ht.erase("a"), 1);
    ht.debug();
    auto it = ht.find("a");
    EXPECT_TRUE(it == ht.end());
    EXPECT_EQ(ht["b"], 20);
    EXPECT_EQ(ht.at("c"), 30);
    EXPECT_EQ(ht["d"], 40);

    it = ht.find("b");
    EXPECT_EQ(it->second, 20);
    EXPECT_EQ(it->first, "b");
    ht.erase(it);
    it = ht.find("b");
    EXPECT_TRUE(it == ht.end());
    ht.debug();
    EXPECT_EQ(ht.size(), 2u);
    EXPECT_EQ(ht["c"], 30);
    EXPECT_EQ(ht["d"], 40);
}

TEST(pool_hash_map, insert_erase_ok) {
    pool_hash_map<std::string, int> ht;
    ht["a"] = 10;
    EXPECT_EQ(ht.size(), 1u);
    
    std::pair<const std::string, int> p1{"b", 20};
    auto p = ht.insert(p1);
    EXPECT_TRUE(p.second);
    EXPECT_EQ(ht.size(), 2u);
    
    std::pair<const std::string, int> p2{"c", 30};
    p = ht.insert(std::move(p2));
    EXPECT_TRUE(p.second);
    EXPECT_EQ(ht.size(), 3u);

    p = ht.emplace("d", 40);
    EXPECT_TRUE(p.second);
    EXPECT_EQ(ht.size(), 4u);
    EXPECT_EQ(ht["a"], 10);
    EXPECT_EQ(ht["b"], 20);
    EXPECT_EQ(ht["c"], 30);
    EXPECT_EQ(ht["d"], 40);

    // repeated erase and insert
    for (size_t i = 0; i < 100; i++) {
        ht.erase("b");
        EXPECT_EQ(ht.size(), 3u);
        ht.insert(p1);
        EXPECT_EQ(ht.size(), 4u);
    }

    //ht.max_load_factor(100.0f);
    for (size_t i = 0; i < 20000; i++) {
        ht.emplace(format_string("%d", i), i);
        EXPECT_EQ(ht.size(), 5u + i);
    }
    EXPECT_EQ(ht.size(), 20004u);

    //ht.dump();
    for (size_t i = 0; i < 20000u; i++) {
        EXPECT_EQ(ht[format_string("%d", i)], (int)i);
        ht.erase(format_string("%d", i));
    }
    //ht.dump_stats();

    EXPECT_EQ(ht.size(), 4u);
    EXPECT_EQ(ht["a"], 10);
    EXPECT_EQ(ht["b"], 20);
    EXPECT_EQ(ht["c"], 30);
    EXPECT_EQ(ht["d"], 40);
    size_t count = 0;
    for (auto pair : ht) {
        count++;
    }
    EXPECT_EQ(count, 4u);
}

TEST(pool_hash_map, const_ok) {
    pool_hash_map<std::string, int> ht;
    ht["a"] = 10;
    ht["b"] = 20;
    ht["c"] = 30;
    ht["d"] = 40;

    const pool_hash_map<std::string, int> ht2 = ht;

    size_t count = 0;
    for (auto it = ht2.begin(); it != ht2.end(); ++it) {
        count++;
    }
    EXPECT_EQ(count, 4u);


    auto it = ht2.find("a");
    EXPECT_EQ(it->second, 10);

    it = ht2.find("e");
    EXPECT_TRUE(it == ht2.end());

    EXPECT_EQ(ht2.at("d"), 40);
    bool is_same_flag = std::is_same<decltype(ht.at("d")), int&>::value;
    EXPECT_TRUE(is_same_flag);
    
    bool is_same_flag2 = std::is_same<decltype(ht2.at("d")), const int&>::value;
    EXPECT_TRUE(is_same_flag2);
}

TEST(pool_hash_map, emplace_ok) {
    pool_hash_map<int, std::string> ht;
    //ht.emplace(1, "====");
    ht.emplace(std::piecewise_construct, std::forward_as_tuple(1), std::forward_as_tuple(4, '='));
    EXPECT_STREQ(ht[1].c_str(), "====");

    ht.emplace(2, "1234");
    EXPECT_STREQ(ht[2].c_str(), "1234");
}

TEST(pool_hash_map, copy_swap_move_ok) {
    pool_hash_map<std::string, int> hta;
    hta["a"] = 10;
    hta["b"] = 20;
    hta["c"] = 30;
    hta["d"] = 40;
    hta.debug();

    pool_hash_map<std::string, int> htb = hta;
    EXPECT_EQ(htb["a"], hta["a"]);
    EXPECT_EQ(htb["b"], hta["b"]);
    EXPECT_EQ(htb["c"], hta["c"]);
    EXPECT_EQ(htb["d"], hta["d"]);
    EXPECT_EQ(htb.size(), 4u);
    htb.debug();

    //htb.dump_stats();

    htb.clear();
    htb.clear();
    EXPECT_EQ(htb.size(), 0u);

    htb.emplace("e", 50);
    std::swap(htb, hta);
    EXPECT_EQ(htb.size(), 4u);
    EXPECT_EQ(hta.size(), 1u);

    EXPECT_EQ(htb["a"], 10);
    EXPECT_EQ(htb["b"], 20);
    EXPECT_EQ(htb["c"], 30);
    EXPECT_EQ(htb["d"], 40);

    EXPECT_EQ(hta["e"], 50);

    pool_hash_map<std::string, int> htc = std::move(hta);
    htc.debug();
    EXPECT_EQ(htc.size(), 1u);
    EXPECT_EQ(hta.size(), 0u);
    EXPECT_EQ(htc["e"], 50);
}

TEST(pool_hash_map, erase_if_ok) {
    pool_hash_map<std::string, int> ht;
    for (size_t i = 0; i < 1000; ++i) {
        ht[std::to_string(i)] = i;
    }
    for (auto it = ht.begin(); it != ht.end();) {
        if (it->second % 2) {
            it = ht.erase(it);
        } else {
            ++it;
        }
    }
    EXPECT_EQ(ht.size(), 500);
    for (size_t i = 0; i < 1000; i += 2) {
        EXPECT_EQ(ht[std::to_string(i)], i);
    }
    EXPECT_EQ(ht.size(), 500);
}


void random(int& x) {
    x = rand() % 10000;
}

void random(std::string& str) {
    int x;
    random(x);
    str = std::to_string(x);
};

void random(test_string& str) {
    int x;
    random(x);
    str = std::to_string(x);
};

template<class K, class T, class HASH> void full_test(pool_hash_map<K, T, HASH>& mp1,
    google::dense_hash_map<K, T, HASH>& mp2, int insert, int erase, int find, int set) {
    
    std::vector<int> ops;
    for (int i = 0; i < insert; ++i) {
        ops.push_back(0);
    }
    for (int i = 0; i < erase; ++i) {
        ops.push_back(1);
    }
    for (int i = 0; i < find; ++i) {
        ops.push_back(2);
    }
    for (int i = 0; i < set; ++i) {
        ops.push_back(3);
    }
    std::random_shuffle(ops.begin(), ops.end());


    for (int op: ops) {
        K key;
        T value;
        random(key);
        random(value);
        // SLOG(INFO) << op << ' ' << key << ' ' << value;
        bool move = rand() % 2;
        if (op == 0) {
            K key2 = key;
            T value2 = value;
            int detail = rand() % 4;
            if (detail == 0) {
                auto res1 = mp1.insert({key, value});
                auto res2 = mp2.insert({key, value});
                ASSERT_EQ(res1.second, res2.second);
                ASSERT_EQ(*res1.first, *res2.first);
            } else if (detail == 1) {
                auto res1 = move ? mp1.emplace(std::move(key), std::move(value)) : mp1.emplace(key, value);
                auto res2 = mp2.insert({key2, value2});
                ASSERT_EQ(res1.second, res2.second);
                ASSERT_EQ(*res1.first, *res2.first);
            } else if (detail == 2) {
                auto res1 = move ? mp1.try_emplace(std::move(key)) : mp1.try_emplace(key);
                if (!mp2.count(key)) {
                    ASSERT_EQ(res1.first->second, T());
                    res1.first->second = std::move(value);
                }
                auto res2 = mp2.insert({key2, value2});
                ASSERT_EQ(res1.second, res2.second);
                ASSERT_EQ(*res1.first, *res2.first);
            } else {
                // result not same with 0 1 2
                if (move) {
                    mp1[std::move(key)] = std::move(value);
                } else {
                    mp1[key] = value;
                }
                mp2[key2] = value2;
            }
        } else if (op == 1) {
            int detail = rand() % 2;
            if (detail == 0) {
                ASSERT_EQ(mp1.erase(key), mp2.erase(key));
            } else {
                auto it = mp1.find(key);
                if (it != mp1.end()) {
                    auto it = mp1.erase(mp1.find(key));
                    SCHECK(it == mp1.end() || it->first != key);
                }
                mp2.erase(key);
            }
        } else if (op == 2) {
            int detail = rand() % 2;
            if (detail == 0) {
                auto it1 = mp1.find(key);
                auto it2 = mp2.find(key);
                if (it2 != mp2.end()) {
                    ASSERT_EQ(it1->second, mp1.at(key));
                    ASSERT_EQ(*it1, *it2);
                }
            } else {
                ASSERT_EQ(mp1.count(key), mp2.count(key));
            }
        } else if (op == 3) {
            ASSERT_EQ(mp1[key], mp2[key]);
            mp1[key] = value;
            mp2[key] = value;
        }
        ASSERT_EQ(mp1.size(), mp2.size());
    }
    

    mp1 = mp1;
    mp1 = std::move(mp1);
    std::swap(mp1, mp1);
    
    auto mp11 = mp1;
    std::swap(mp11, mp1);
    ASSERT_EQ(mp11.size(), mp1.size());
    for (auto& pair: mp1) {
        ASSERT_EQ(mp11.at(pair.first), pair.second);
    }
    for (auto& pair: mp11) {
        ASSERT_EQ(mp1.at(pair.first), pair.second);
    }
    ASSERT_EQ(mp11.size(), mp1.size());

    mp1.rehash(0);
    ASSERT_EQ(mp1.load_factor(), 1.0);
    auto it = mp11.begin();
    while (it != mp11.end()) {
        it = mp11.erase(it);
    }
    ASSERT_EQ(mp11.size(), 0);
    ASSERT_EQ(mp11.load_factor(), 0);
    mp11.reserve(mp1.size());
    ASSERT_EQ(mp11.bucket_count(), mp1.bucket_count());

    for (auto& pair: mp2) {
        ASSERT_EQ(mp1[pair.first], mp2[pair.first]);
        ASSERT_EQ(mp1.at(pair.first), mp2[pair.first]);
        ASSERT_EQ(mp1.find(pair.first)->second, mp2[pair.first]);
        ASSERT_EQ(mp1.count(pair.first), mp2.count(pair.first));
    }
    for (auto& pair: mp1) {
        ASSERT_EQ(mp1[pair.first], mp2[pair.first]);
        ASSERT_EQ(mp1.at(pair.first), mp2[pair.first]);
        ASSERT_EQ(mp1.find(pair.first)->second, mp2[pair.first]);
        ASSERT_EQ(mp1.count(pair.first), mp2.count(pair.first));
    }

};

TEST(pool_hash_map, full_test_int_int) {
    pool_hash_map<int, int> mp1;
    google::dense_hash_map<int, int> mp2;
    mp2.set_empty_key(-1);
    mp2.set_deleted_key(-2);

    full_test(mp1, mp2, 20000, 10000, 10000, 10000);
    full_test(mp1, mp2, 10000, 50000, 10000, 10000);

    full_test(mp1, mp2, 20000, 10000, 10000, 10000);
    full_test(mp1, mp2, 0, 10000, 0, 0);
    full_test(mp1, mp2, 10000, 50000, 10000, 10000);
}

TEST(pool_hash_map, full_test_string_string) {
    pool_hash_map<std::string, std::string> mp1;
    google::dense_hash_map<std::string, std::string> mp2;
    mp2.set_empty_key("-1");
    mp2.set_deleted_key("-2");
    
    full_test(mp1, mp2, 20000, 10000, 10000, 10000);
    full_test(mp1, mp2, 10000, 50000, 10000, 10000);

    full_test(mp1, mp2, 20000, 10000, 10000, 10000);
    full_test(mp1, mp2, 0, 10000, 0, 0);
    full_test(mp1, mp2, 10000, 50000, 10000, 10000);
}

TEST(pool_hash_map, no_object_leak) {
    pool_hash_map<test_string, test_string, test_string_hash> mp1;
    {
        google::dense_hash_map<test_string, test_string, test_string_hash> mp2;
        mp2.set_empty_key("-1");
        mp2.set_deleted_key("-2");
        
        full_test(mp1, mp2, 20000, 10000, 10000, 10000);
        full_test(mp1, mp2, 10000, 50000, 10000, 10000);

        full_test(mp1, mp2, 20000, 10000, 10000, 10000);
        full_test(mp1, mp2, 0, 10000, 0, 0);
        full_test(mp1, mp2, 10000, 50000, 10000, 10000);
    }
    mp1.clear();
    EXPECT_EQ(g_count, 0);
}


} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
