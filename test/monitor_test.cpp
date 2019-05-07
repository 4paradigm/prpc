//
// Created by 孙承根 on 2017/9/15.
//

#include <glog/logging.h>
#include <gtest/gtest.h>
#include "Monitor.h"
#include "common.h"
#include "macro.h"
#include "pico_lexical_cast.h"
#include "pico_memory.h"
#include "pico_test_common.h"
#include "pico_unittest_operator.h"
#define VAR(v) " " << #v << "=" << (v)

void add_task(paradigm4::pico::Monitor& monitor, int a) {
    auto id = monitor.submit(PICO_FILE_LINENUM, 0, 2000, [=] {
        LOG(INFO) << "add " << a;
        sleep(2);
    });
    monitor.destroy(id);
}

TEST(monitor, submit) {
    using paradigm4::pico::Monitor;
    Monitor& monitor = Monitor::singleton();
    auto id = monitor.submit(PICO_FILE_LINENUM, 0, 1000, [] {
        static int count = 0;
        LOG(INFO) << count++;
    });
    auto id2 = monitor.submit(PICO_FILE_LINENUM, 0, 5000, [] {
        LOG(INFO) << "2";
        sleep(2);
    });
    add_task(monitor, 4);
    add_task(monitor, 5);

    sleep(10);
    monitor.destroy(id);
    monitor.destroy(id2);
}

TEST(monitor, destroy) {
    using paradigm4::pico::Monitor;
    Monitor& monitor = Monitor::singleton();
    auto id          = monitor.submit(PICO_FILE_LINENUM, 0, 500, [] {
        static int count = 0;
        LOG(INFO) << "b " << count;
        sleep(4);
        LOG(INFO) << "e " << count++;

    });
    sleep(3);
    LOG(INFO) << "destroy";
    auto waiter = monitor.destroy(id);
    LOG(INFO) << "wait destroy";
    EXPECT_TRUE(waiter.wait());
    EXPECT_TRUE(monitor.query(id) == Monitor::DONE);
    LOG(INFO) << "wait destroy finish";

    sleep(10);
}
TEST(monitor, destroy_with_additional_run) {
    using paradigm4::pico::Monitor;
    Monitor& monitor = Monitor::singleton();
    auto id          = monitor.submit(PICO_FILE_LINENUM, 0, 500, [] {
        static int count = 0;
        LOG(INFO) << "b " << count;
        sleep(4);
        LOG(INFO) << "e " << count++;

    });
    sleep(3);
    LOG(INFO) << "destroy_with_additional_run";
    auto waiter = monitor.destroy_with_additional_run(id);
    LOG(INFO) << "destroy_with_additional_run start wait";
    EXPECT_TRUE(waiter.wait());
    EXPECT_TRUE(monitor.query(id) == Monitor::DONE);
    LOG(INFO) << "destroy_with_additional_run finish wait";

    sleep(10);
}

int main(int argc, char* argv[]) {

    testing::InitGoogleTest(&argc, argv);
    google::AllowCommandLineReparsing();
    google::ParseCommandLineFlags(&argc, &argv, false);
    paradigm4::pico::LogReporter::initialize();

    paradigm4::pico::test::PicoUnitTestCommon::singleton().initialize(&argc, argv);
    if (paradigm4::pico::test::PicoUnitTestOperator::singleton().is_show_operator()) {
        // no_wrapper, repeat_num=1
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(
            paradigm4::pico::test::NoWrapperOperator(1));
        paradigm4::pico::test::PicoUnitTestOperator::singleton().show_operator();
        paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
        return 0;
    }
    paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
    int ret = RUN_ALL_TESTS();

    paradigm4::pico::LogReporter::finalize();
    return ret;
}
