#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "pico_test_common.h"
#include "pico_unittest_operator.h"
#include "common.h"

namespace paradigm4 {
namespace pico {

TEST(VectorMoveAppend, ok) {
    std::vector<std::vector<int>> vect = {{3, 4}, {5}, {6, 7, 8}};
    std::vector<std::vector<int>> result = {{0}, {1, 2}};

    vector_move_append(result, vect);

    int i = 0;
    for (auto& v : result) {
        for (auto& e : v) {
            EXPECT_EQ(i++, e);
        }
    }

    EXPECT_EQ(vect.size(), 3u);

    for (auto& v : vect) {
        EXPECT_EQ(v.size(), 0u);
    }
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
