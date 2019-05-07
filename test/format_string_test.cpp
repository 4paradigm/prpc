#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "pico_test_common.h"
#include "pico_unittest_operator.h"
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
    paradigm4::pico::test::PicoUnitTestCommon::singleton().initialize(&argc, argv);
    if (paradigm4::pico::test::PicoUnitTestOperator::singleton().is_show_operator()) {
        // no_wrapper, repeat_num=1
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(paradigm4::pico::test::NoWrapperOperator(1));
        paradigm4::pico::test::PicoUnitTestOperator::singleton().show_operator();
        paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
        return 0;
    }
    int ret = RUN_ALL_TESTS();
    paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
    return ret;
}
