#include <cstdlib>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include "common/include/common.h"
#include "common/include/initialize.h"
#include "pico_test_common.h"
#include "pico_unittest_operator.h"
#include "common/include/URIConfig.h"

namespace paradigm4 {
namespace pico {

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

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    paradigm4::pico::test::PicoUnitTestCommon::singleton().initialize(&argc, argv);
    if (paradigm4::pico::test::PicoUnitTestOperator::singleton().is_show_operator()) {
        // no_wrapper, repeat_num=1
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(paradigm4::pico::test::NoWrapperOperator(1));
        paradigm4::pico::test::PicoUnitTestOperator::singleton().show_operator();
        paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
        return 0;
    }
    //paradigm4::pico::pico_initialize(argc, argv);
    int ret = RUN_ALL_TESTS();
    //paradigm4::pico::pico_finalize();
    paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
    return ret; 
}

