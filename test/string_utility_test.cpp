#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "StringUtility.h"

namespace paradigm4 {
namespace pico {

TEST(StringUtility, check_split_ok) {
    std::string text  = "0,1,2,3,4,5,6,7,8,9";
    std::vector<std::pair<char*, size_t>> token;
    StringUtility::split(text, token, ',');
    ASSERT_EQ(10u, token.size());
    for (size_t i = 0; i < token.size(); i++) {
        EXPECT_STREQ(std::to_string(i).c_str(), token[i].first);
        EXPECT_EQ(1u, token[i].second);
    }
}

TEST(StringUtility, check_split_with_empty_token_ok) {
    std::string text  = ",,0,,1,2,,";
    std::vector<std::pair<char*, size_t>> token;
    StringUtility::split(text, token, ',');
    ASSERT_EQ(8u, token.size());
    EXPECT_STREQ("", token[0].first);
    EXPECT_STREQ("", token[1].first);
    EXPECT_STREQ("0", token[2].first);
    EXPECT_STREQ("", token[3].first);
    EXPECT_STREQ("1", token[4].first);
    EXPECT_STREQ("2", token[5].first);
    EXPECT_STREQ("", token[6].first);
    EXPECT_STREQ("", token[7].first);
}

TEST(StringUtility, check_split_empty_ok) {
    std::string text  = "";
    std::vector<std::pair<char*, size_t>> token;
    StringUtility::split(text, token, ',');
    ASSERT_EQ(1u, token.size());
    EXPECT_STREQ("", token[0].first);
}

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
