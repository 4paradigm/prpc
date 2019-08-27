#include <pico-core/MemoryStorage.h>
#include <cstdlib>
#include <glog/logging.h>
#include <gtest/gtest.h>


namespace paradigm4 {
namespace pico {
namespace core {

TEST(MemoryStorage, ReadWrite) {
    {
        auto v1 = MemoryStorage::singleton().open_write<std::vector<int>>("/v1");
        auto v2 = MemoryStorage::singleton().open_write<std::vector<std::string>>("/v2");
        for (int i = 0; i < 100; ++i) {
            v1->push_back(i + 1);
            v2->push_back(std::to_string(i));
        }
        MemoryStorage::singleton().close("/v1");
        MemoryStorage::singleton().close("/v2");
    }

    {
        auto v1 = MemoryStorage::singleton().open_read<std::vector<int>>("//v1");
        auto v2 = MemoryStorage::singleton().open_read<std::vector<std::string>>("/v2");
        for (int i = 0; i < 100; ++i) {
            EXPECT_EQ(i + 1, (*v1)[i]);
            EXPECT_EQ(std::to_string(i), (*v2)[i]);
        }
        MemoryStorage::singleton().close("/v1");
        MemoryStorage::singleton().close("/v2");
    }
}

TEST(MemoryStorage, Op) {
    MemoryStorage ms;
    auto s1 = ms.open_write<int>("/a/b");
    EXPECT_NE(s1, nullptr);
    *s1 = 1;
    auto s2 = ms.open_write<int>("/a");
    EXPECT_NE(s2, nullptr);
    *s2 = 2;

    auto r1 = ms.open_read<int>("/a/b");
    EXPECT_EQ(r1, nullptr);
    ms.close("/a/b");
    r1 = ms.open_read<int>("/a/b");
    EXPECT_NE(r1, nullptr);
    EXPECT_EQ(*r1, 1);
    ms.close("/a/b");

    auto r2 = ms.open_read<int>("/a");
    EXPECT_EQ(r2, nullptr);
    ms.close("/a");
    r2 = ms.open_read<int>("/a");
    EXPECT_NE(r2, nullptr);
    EXPECT_EQ(*r2, 2);
    r1 = ms.open_read<int>("/a");
    EXPECT_NE(r1, nullptr);
    EXPECT_EQ(r1, r2);
    ms.close("/a");
    EXPECT_TRUE(ms.open_write<int>("/a") == nullptr);
    ms.close("/a");
    EXPECT_TRUE(ms.open_write<int>("/a") != nullptr);

    ms.rmr("/a");
    EXPECT_TRUE(ms.open_read<int>("/a") == nullptr);
    EXPECT_TRUE(ms.open_read<int>("/a/b") == nullptr);

    auto v1 = ms.open_write<std::vector<std::string>>("/ab/c");
    for (int i = 0; i < 100; ++i) {
        v1->push_back(std::to_string(50 - i));
    }
    ms.close("/ab\\////c");
    auto v2 = ms.open_read<std::vector<std::string>>("\\ab\\c");
    EXPECT_NE(v2, nullptr);
    EXPECT_EQ(v2->size(), 100);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ((*v2)[i], std::to_string(50 - i));
    }

    EXPECT_FALSE(ms.exists("/a/1/2"));
    EXPECT_TRUE(ms.mkdir_p("/a/1/2"));
    EXPECT_FALSE(ms.mkdir_p("/a/1/2"));
    EXPECT_TRUE(ms.exists("/a/1/2"));
    EXPECT_TRUE(ms.rmr("/a/1/2"));
    EXPECT_FALSE(ms.exists("/a/1/2"));
}

void write_v1(const std::string& path) {
    auto v1 = MemoryStorage::singleton().open_write<std::vector<int>>(path);
    for (int i = 0; i < 100; ++i) {
        v1->push_back(i + 1);
    }
    MemoryStorage::singleton().close(path);
}

void read_v1(const std::string& path) {
    auto v1 = MemoryStorage::singleton().open_read<std::vector<int>>(path);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(i + 1, (*v1)[i]);
    }
    MemoryStorage::singleton().close(path);
}

TEST(MemoryStorage, Tree) {
    write_v1("a");
    read_v1("/a");
    read_v1("\\a");
    read_v1("/a//////\\");
    read_v1("//////a");

    write_v1("./b");
    read_v1("./b");
    read_v1("./////b");
    read_v1("./\\\\b");
    read_v1("./b\\\\\\////");

    write_v1("1/2/3\\4\\5");
    read_v1("1/2/3\\4\\5");
    read_v1("/1//2//3\\4\\5//");
}

}
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

