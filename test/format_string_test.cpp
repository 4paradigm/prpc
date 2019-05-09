#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "common.h"

namespace paradigm4 {
namespace pico {

TEST(FormatString, no_parameter) {
    std::string str = format_string();
    EXPECT_STREQ(str.c_str(), "");
}

TEST(FormatString, test_ok) {
    std::string str = format_string("test : %d : %s", 10, "abc");
    EXPECT_STREQ(str.c_str(), "test : 10 : abc");
    
    str = format_string("test");
    EXPECT_STREQ(str.c_str(), "test");
}

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
