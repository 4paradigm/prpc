#include "glog/logging.h"
#include "gtest/gtest.h"

#include "LRUCache.h"

namespace paradigm4 {
namespace pico {
namespace core {

TEST(LRUCache, basic_ops) {
    LRUCache<size_t, size_t> cache(4);
    size_t value = 0;
    bool found = false;

    cache.insert(1, 1);
    EXPECT_EQ(1u, cache.item_count());
    EXPECT_EQ(8u, cache.item_memory_usage());
    found = cache.find(1, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(1, value);

    cache.insert(2, 2);
    EXPECT_EQ(2u, cache.item_count());
    EXPECT_EQ(16u, cache.item_memory_usage());
    found = cache.find(2, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(2, value);

    cache.insert(3, 3);
    EXPECT_EQ(3u, cache.item_count());
    EXPECT_EQ(24u, cache.item_memory_usage());
    found = cache.find(3, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(3, value);

    cache.insert(4, 4);
    EXPECT_EQ(4u, cache.item_count());
    EXPECT_EQ(32u, cache.item_memory_usage());
    found = cache.find(4, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(4, value);

    cache.insert(5, 5); // 1 is evicted
    EXPECT_EQ(4u, cache.item_count());
    EXPECT_EQ(32u, cache.item_memory_usage());
    found = cache.find(1, value);
    EXPECT_FALSE(found);
    found = cache.find(5, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(5, value);

    found = cache.find(2, value);
    EXPECT_EQ(2, value);
    EXPECT_TRUE(found);

    found = cache.find(2, value);
    EXPECT_EQ(2, value);
    EXPECT_TRUE(found);

    cache.insert(6, 6); // 3 is evicted
    found = cache.find(2, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(2, value);
    found = cache.find(3, value);
    EXPECT_FALSE(found);

    cache.clear();
    EXPECT_EQ(0u, cache.item_count());
    EXPECT_EQ(0u, cache.item_memory_usage());
    cache.insert(7, 7); 
    EXPECT_EQ(1u, cache.item_count());
    EXPECT_EQ(8u, cache.item_memory_usage());
    found = cache.find(7, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(7, value);

    cache.clear();
    EXPECT_EQ(0u, cache.item_count());
    EXPECT_EQ(0u, cache.item_memory_usage());
}

TEST(LRUCache, size_limited) {
    LRUCache<size_t, size_t> cache(4, 16, 0, 0);
    size_t value = 0;
    bool found = false;

    cache.insert(1, 1);
    EXPECT_EQ(1u, cache.item_count());
    EXPECT_EQ(8u, cache.item_memory_usage());
    found = cache.find(1, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(1, value);

    cache.insert(2, 2);
    EXPECT_EQ(2u, cache.item_count());
    EXPECT_EQ(16u, cache.item_memory_usage());
    found = cache.find(2, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(2, value);

    cache.insert(3, 3); // 1 is evicted
    EXPECT_EQ(2u, cache.item_count());
    EXPECT_EQ(16u, cache.item_memory_usage());
    found = cache.find(3, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(3, value);

    found = cache.find(1, value);
    EXPECT_FALSE(found);

    cache.clear();
    EXPECT_EQ(0u, cache.item_count());
    EXPECT_EQ(0u, cache.item_memory_usage());

    cache.insert(7, 7); 
    EXPECT_EQ(1u, cache.item_count());
    EXPECT_EQ(8u, cache.item_memory_usage());

    found = cache.find(7, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(7, value);

    cache.clear();
    EXPECT_EQ(0u, cache.item_count());
    EXPECT_EQ(0u, cache.item_memory_usage());
}

struct double_sized_value_memory_usage_fn {
    size_t operator()(const size_t& value) const {
        return 2 * sizeof(value);
    }
};

TEST(LRUCache, customized_memory_usage_fn) {
    LRUCache<size_t, size_t, double_sized_value_memory_usage_fn> cache(4, 16, 0, 0);
    size_t value = 0;
    bool found = false;

    cache.insert(1, 1);
    EXPECT_EQ(1u, cache.item_count());
    EXPECT_EQ(16u, cache.item_memory_usage());
    found = cache.find(1, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(1, value);

    cache.insert(2, 2); // 1 is evicted
    EXPECT_EQ(1u, cache.item_count());
    EXPECT_EQ(16u, cache.item_memory_usage());
    found = cache.find(2, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(2, value);

    found = cache.find(1, value);
    EXPECT_FALSE(found);

    cache.clear();
    EXPECT_EQ(0u, cache.item_count());
    EXPECT_EQ(0u, cache.item_memory_usage());

    cache.insert(3, 3); 
    EXPECT_EQ(1u, cache.item_count());
    EXPECT_EQ(16u, cache.item_memory_usage());

    found = cache.find(3, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(3, value);

    cache.clear();
    EXPECT_EQ(0u, cache.item_count());
    EXPECT_EQ(0u, cache.item_memory_usage());
}

TEST(LRUCache, access_count_limited) {
    LRUCache<size_t, size_t> cache(4, 0, 0, 2); // 最多可以读两次
    size_t value = 0;
    bool found = false;

    cache.insert(1, 1);
    EXPECT_EQ(1u, cache.item_count());
    EXPECT_EQ(8u, cache.item_memory_usage());
    found = cache.find(1, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(1, value);

    cache.insert(2, 2);
    EXPECT_EQ(2u, cache.item_count());
    EXPECT_EQ(16u, cache.item_memory_usage());

    found = cache.find(2, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(2, value);

    found = cache.find(2, value); // 此次读完，key 2 将被 evict
    EXPECT_TRUE(found);
    EXPECT_EQ(2, value);

    found = cache.find(2, value);
    EXPECT_FALSE(found);

    found = cache.find(1, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(1, value);
}

TEST(LRUCache, item_ttl_limited) {
    LRUCache<size_t, size_t> cache(4, 0, 1, 0); // 1s 后元素会 evict
    size_t value = 0;
    bool found = false;

    cache.insert(1, 1);
    EXPECT_EQ(1u, cache.item_count());
    EXPECT_EQ(8u, cache.item_memory_usage());
    found = cache.find(1, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(1, value);

    cache.insert(2, 2);
    EXPECT_EQ(2u, cache.item_count());
    EXPECT_EQ(16u, cache.item_memory_usage());

    found = cache.find(2, value);
    EXPECT_TRUE(found);
    EXPECT_EQ(2, value);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1s);

    found = cache.find(2, value); // 此次读完，key 2 将被 evict
    EXPECT_TRUE(found);
    EXPECT_EQ(2, value);

    found = cache.find(2, value);
    EXPECT_FALSE(found);

    found = cache.find(1, value); // 此次读完，key 1 将被 evict;
    EXPECT_TRUE(found);
    EXPECT_EQ(1, value);

    found = cache.find(1, value);
    EXPECT_FALSE(found);
}

}  // core
}  // pico
}  // paradigm4