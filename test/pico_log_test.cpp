#include <cstdlib>
#include <cstdio>
#include <queue>
#include <string>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "pico_test_common.h"
#include "pico_unittest_operator.h"

#include "ThreadGroup.h"

#include "pico_log.h"

namespace paradigm4 {
namespace pico {

TEST(picoLog, check_ok) {
    RLOG(INFO) << "hello world\"'!@#$%^&*()_+";
    BLOG(2) << "debug log";
}

TEST(picoLog, multithread_ok) {
    ThreadGroup tg(10);
    std::vector<AsyncReturn> rets;

    for (int i = 0; i < 100; i++) {
        auto ret = tg.async_exec([i](int tid) {
                    RLOG(INFO) << "hello world " << i << " " << tid;
                });
        rets.push_back(ret);
    }
    for (int i = 0; i < 100; i++) {
        rets[i].wait();
    }
}

}
}

int main(int argc, char* argv[]) {
    STDERR_LOG() << "STDERR_LOG";
    testing::InitGoogleTest(&argc, argv);
    google::AllowCommandLineReparsing();
    google::ParseCommandLineFlags(&argc, &argv, false);
    paradigm4::pico::LogReporter::initialize();
    
    paradigm4::pico::test::PicoUnitTestCommon::singleton().initialize(&argc, argv);
    if (paradigm4::pico::test::PicoUnitTestOperator::singleton().is_show_operator()) {
        // no_wrapper, repeat_num=1
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(paradigm4::pico::test::NoWrapperOperator(1));
        paradigm4::pico::test::PicoUnitTestOperator::singleton().show_operator();
        paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
        return 0;
    }
    paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
    int ret = RUN_ALL_TESTS();

    paradigm4::pico::LogReporter::finalize();
    return ret;
}
