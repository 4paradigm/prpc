#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "common.h"

namespace paradigm4 {
namespace pico {
namespace core {

TEST(FileLineReader, check_ok) {
    FILE* file = std::tmpfile();
    CHECK(file);
    std::fputs("a\nb\nc\n\n", file);
    std::rewind(file);
    FileLineReader reader;
    EXPECT_TRUE(reader.getline(file));
    EXPECT_STREQ(reader.buffer(), "a");
    EXPECT_TRUE(reader.getline(file));
    EXPECT_STREQ(reader.buffer(), "b");
    EXPECT_TRUE(reader.getline(file));
    EXPECT_STREQ(reader.buffer(), "c");
    EXPECT_TRUE(reader.getline(file));
    EXPECT_STREQ(reader.buffer(), "");
    EXPECT_FALSE(reader.getline(file));

    FILE* file2 = std::tmpfile();
    CHECK(file2);
    EXPECT_FALSE(reader.getline(file2));
}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
