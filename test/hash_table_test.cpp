#include <cstdlib>
#include <cstdio>
#include <string>
#include <type_traits>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "common.h"
#include "HashTable.h"

namespace paradigm4 {
namespace pico {
namespace core {

TEST(HashTable, default_constructor_ok) {
    HashTable<std::string, int> ht;
    EXPECT_EQ(ht.size(), 0u);
    EXPECT_EQ(ht.bucket_count(), 1u);
    EXPECT_EQ(ht.bucket_capacity(), 1u);
}

TEST(HashTable, capacity_constructor_ok) {
    HashTable<std::string, int> ht(10);
    EXPECT_EQ(ht.size(), 0u);
    EXPECT_EQ(ht.bucket_count(), 1u);
    EXPECT_EQ(ht.bucket_capacity(), 10u);
}

TEST(HashTable, initializer_list_constructor_ok) {
    HashTable<std::string, int> ht{{"a", 10}, {"b", 20}, {"c", 30}, {"d", 40}};
    EXPECT_EQ(ht.size(), 4u);
    EXPECT_EQ(ht["a"], 10);
    EXPECT_EQ(ht["b"], 20);
    EXPECT_EQ(ht["c"], 30);
    EXPECT_EQ(ht.at("d"), 40);
}

TEST(HashTable, erase_ok) {
    HashTable<std::string, int> ht{{"a", 10}, {"b", 20}, {"c", 30}, {"d", 40}};
    ht.erase("a");
    auto it = ht.find("a");
    EXPECT_TRUE(it == ht.end());
    EXPECT_EQ(ht["b"], 20);
    EXPECT_EQ(ht.at("c"), 30);
    EXPECT_EQ(ht["d"], 40);

    it = ht.find("b");
    ht.erase("b");
    it = ht.find("b");
    EXPECT_TRUE(it == ht.end());
    EXPECT_EQ(ht.size(), 2u);
    EXPECT_EQ(ht["c"], 30);
    EXPECT_EQ(ht["d"], 40);

    ht.erase(ht.begin(), ht.end());
    it = ht.find("c");
    EXPECT_TRUE(it == ht.end());
    EXPECT_EQ(ht.size(), 0u);

    ht["c"] = 30;
    ht["d"] = 40;
    EXPECT_EQ(ht.size(), 2u);
    ht.erase(ht.cbegin(), ht.cend());
    it = ht.find("c");
    EXPECT_TRUE(it == ht.end());
    EXPECT_EQ(ht.size(), 0u);
}

TEST(HashTable, insert_erase_ok) {
    HashTable<std::string, int> ht;
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

TEST(HashTable, local_iterator_ok) {
    HashTable<std::string, int> ht;
    ht["a"] = 10;
    ht["b"] = 20;
    ht["c"] = 30;
    ht["d"] = 40;

    size_t count = 0;
    for (size_t i = 0; i < ht.bucket_count(); ++i) {
        for (auto it = ht.begin(i); it != ht.end(i); ++it) {
            count++;
        }
    }
    EXPECT_EQ(count, 4u);

    count = 0;
    for (size_t i = 0; i < ht.bucket_count(); ++i) {
        for (auto it = ht.cbegin(i); it != ht.cend(i); ++it) {
            count++;
        }
    }
    EXPECT_EQ(count, 4u);
}

TEST(HashTable, const_ok) {
    HashTable<std::string, int> ht;
    ht["a"] = 10;
    ht["b"] = 20;
    ht["c"] = 30;
    ht["d"] = 40;

    const HashTable<std::string, int> ht2 = ht;

    size_t count = 0;
    for (auto it = ht2.begin(); it != ht2.end(); ++it) {
        count++;
    }
    EXPECT_EQ(count, 4u);

    count = 0;
    for (size_t i = 0; i < ht2.bucket_count(); ++i) {
        for (auto it = ht2.cbegin(i); it != ht2.cend(i); ++it) {
            count++;
        }
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

    auto entry_pointer = ht2.bucket(ht2.find_first_nonempty(0));
    EXPECT_TRUE(entry_pointer != nullptr);

    ht2.dump();
    ht2.dump_stats();
}

TEST(HashTable, emplace_ok) {
    HashTable<int, std::string> ht;
    //ht.emplace(1, "====");
    ht.emplace(std::piecewise_construct, std::forward_as_tuple(1), std::forward_as_tuple(4, '='));
    EXPECT_STREQ(ht[1].c_str(), "====");

    ht.emplace(2, "1234");
    EXPECT_STREQ(ht[2].c_str(), "1234");
}

TEST(HashTable, copy_swap_move_ok) {
    HashTable<std::string, int> hta;
    hta["a"] = 10;
    hta["b"] = 20;
    hta["c"] = 30;
    hta["d"] = 40;

    HashTable<std::string, int> htb = hta;
    EXPECT_EQ(htb["a"], hta["a"]);
    EXPECT_EQ(htb["b"], hta["b"]);
    EXPECT_EQ(htb["c"], hta["c"]);
    EXPECT_EQ(htb["d"], hta["d"]);
    EXPECT_EQ(htb.size(), 4u);

    //htb.dump_stats();

    htb.clear();
    htb.clear();
    EXPECT_EQ(htb.size(), 0u);

    htb.emplace("e", 50);
    htb.swap(hta);
    EXPECT_EQ(htb.size(), 4u);
    EXPECT_EQ(hta.size(), 1u);

    EXPECT_EQ(htb["a"], 10);
    EXPECT_EQ(htb["b"], 20);
    EXPECT_EQ(htb["c"], 30);
    EXPECT_EQ(htb["d"], 40);

    EXPECT_EQ(hta["e"], 50);

    HashTable<std::string, int> htc = std::move(hta);
    EXPECT_EQ(htc.size(), 1u);
    EXPECT_EQ(hta.size(), 0u);
    EXPECT_EQ(htc["e"], 50);
}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
