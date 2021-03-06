//
// Created by 孙承根 on 2017/9/15.
//

#include <glog/logging.h>
#include <gtest/gtest.h>
#include "common.h"
#include "ArenaAllocator.h"
#include "pico_lexical_cast.h"
#include "pico_memory.h"
#define VAR(v) " " << #v << "=" << (v)

#define KB *1024L
#define MB *1024 KB
#define GB *1024 MB

using namespace paradigm4::pico::core;

TEST(pico_memory, arena) {
    using namespace paradigm4::pico;
    RpcArena rpcArena;
    std::vector<std::thread> ths(1);
    for (auto& th : ths) {
        th = std::thread([&]() {
            for (int i = 0; i < 10000; ++i) {
                size_t aa = (rand() * 3276 + rand());
                aa %= (1024 * 1024 * 8);
                aa += 1;
                auto ptr = rpcArena.allocate(aa);
                memset(ptr, 0, aa);
                rpcArena.deallocate(ptr);
                if (i % 1000 == 0) {
                    // pico_gc();
                    SLOG(INFO) << i;
                }
            }
        });
    }
    for (auto& th : ths) {
        th.join();
    }
    pico_gc();
    pico_gc();
}

TEST(pico_memory, get_physical_size) {
    using namespace paradigm4::pico;
    EXPECT_GT(pico_mem().get_total_pmem(), 0);
    LOG(INFO) << pico_mem().get_total_pmem();
}
TEST(pico_memory, max_memory) {
    using namespace paradigm4::pico;
    pico_mem().set_max_managed_vmem(200 MB);
    pico_print_memstats();

    EXPECT_EQ(pico_mem().get_max_managed_vmem(), 200 MB);
    pico_print_memstats();

    void* p = pico_malloc(201 MB);
    ASSERT_EQ(p, nullptr);
    pico_mem().set_max_managed_vmem(400 MB);
    p = pico_malloc(201 MB);
    ASSERT_NE(p, nullptr);
}
TEST(pico_memory, get_rss) {
    using namespace paradigm4::pico;
    EXPECT_GT(pico_mem().get_used_pmem(), 0);
    EXPECT_TRUE(pico_mem().get_used_pmem() > 0 && pico_mem().get_used_pmem() < 1000 MB);

    LOG(INFO) << "malloc 100MB";
    char* pInt = static_cast<char*>(pico_malloc(100 MB));
    ASSERT_NE(pInt, nullptr);

    pico_print_memstats();
    EXPECT_TRUE(
        pico_mem().get_used_pmem() > (0 MB) && pico_mem().get_used_pmem() < 100 MB);

    LOG(INFO) << "access 100MB";
    for (size_t i = 0; i < 100 MB; ++i) {
        pInt[i] = static_cast<char>(i & 0xff);
    }
    pico_print_memstats();
    EXPECT_TRUE(
        pico_mem().get_used_pmem() > (100 MB) && pico_mem().get_used_pmem() < 200 MB);
}
TEST(pico_memory, limited_allocator) {
    PicoLimitedAllocator<char> charAllocator;
    PicoLimitedAllocator<int> intAllocator;
    struct AAA{};
    PicoLimitedAllocator<char, AAA, true> charAllocator1;

    pico_malloc(1024);
    char* pc  = charAllocator.allocate(1024);
    char* pc1 = charAllocator1.allocate(2048);
    EXPECT_TRUE(pc != nullptr && pc1 != nullptr);
    EXPECT_GE(charAllocator.current_size(), 1024);
    LOG(INFO) << charAllocator.current_size();
    int* pi = intAllocator.allocate(1024);
    EXPECT_TRUE(pi != nullptr);
    EXPECT_GE(charAllocator.current_size(), 1024 + sizeof(int) * 1024);
    EXPECT_GE(charAllocator1.current_size(), 2048);

    EXPECT_GE(MemPool<void>::current_size(), 1024 + sizeof(int) * 1024);
    charAllocator.deallocate(pc, 0);
    EXPECT_GE(MemPool<void>::current_size(), sizeof(int) * 1024);
    intAllocator.deallocate(pi, 0);
    EXPECT_GE(MemPool<void>::current_size(), 0);
    MemPool<AAA>::set_max_size(2048);
    char* pc2 = charAllocator1.allocate(2048);
    EXPECT_TRUE(pc2 == nullptr);
    EXPECT_GE(charAllocator1.current_size(), 2048);
    charAllocator1.deallocate(pc1, 0);
    EXPECT_EQ(charAllocator1.current_size(), 0);
}

#if defined(USE_JEMALLOC)
extern bool je_opt_retain;
#endif

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    google::AllowCommandLineReparsing();
    google::ParseCommandLineFlags(&argc, &argv, false);
#if defined(USE_JEMALLOC)
    je_opt_retain = false;
#endif
    int ret = RUN_ALL_TESTS();
    return ret;
}
