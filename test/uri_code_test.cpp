#include <cstdlib>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include "URICode.h"

namespace paradigm4 {
namespace pico {
namespace core {

TEST(URICode, EncodeDecode) {
    std::string s;
    std::string e;
    
    s = "awk -F\"|\" '{print $1}'";
    e = "awk%20-F%22%7C%22%20%27%7Bprint%20%241%7D%27";
    EXPECT_EQ(e, URICode::encode(s));
    EXPECT_EQ(s, URICode::decode(e));

    s = "--@@";
    e = "--%40%40";
    EXPECT_EQ(e, URICode::encode(s));
    EXPECT_EQ(s, URICode::decode(e));
}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret; 
}

