#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "initialize.h"
#include "Broadcast.h"
#include "macro.h"
#include "pico_unittest_operator.h"

namespace paradigm4 {
namespace pico {

static int g_root = 0;
TEST(AllReduce, non_in_place_ok) {
    auto run_func = []() {
        int data = pico_comm_rank();
        pico_bcast(data, g_root);
        EXPECT_EQ(data, g_root);

        pico_barrier(PICO_FILE_LINENUM);
    };

    std::thread run_thread = std::thread(run_func);
    run_thread.join();
}
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    paradigm4::pico::test::PicoUnitTestCommon::singleton().initialize(&argc, argv);
    if (paradigm4::pico::test::PicoUnitTestOperator::singleton().is_show_operator()) {
        // local_wrapper, repeat_num=3
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(paradigm4::pico::test::LocalWrapperOperator(3, 0));
        // mpirun, repeat_num=3, np={3, 5, 8}
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(paradigm4::pico::test::MpiRunOperator(3, 0, {3, 5, 8}));
        paradigm4::pico::test::PicoUnitTestOperator::singleton().show_operator();
        paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
        return 0;
    }
    paradigm4::pico::pico_initialize(argc, argv);
    srand(0);
    paradigm4::pico::g_root = rand() % paradigm4::pico::pico_comm_size();
    int ret = RUN_ALL_TESTS();
    paradigm4::pico::pico_finalize();
    paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
    return ret;
}
