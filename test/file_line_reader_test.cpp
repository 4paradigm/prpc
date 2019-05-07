#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "pico_test_common.h"
#include "pico_unittest_operator.h"
#include "common.h"

namespace paradigm4 {
namespace pico {

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
