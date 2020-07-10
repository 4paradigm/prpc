#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "macro.h"
#include "ThreadGroup.h"
#include "Accumulator.h"
#include "AccumulatorClient.h"
#include "AccumulatorServer.h"
#include "AutoTimer.h"

namespace paradigm4 {
namespace pico {
namespace core {

TEST(AutoTimerTest, single_thread_framework_auto_timer_ms_time1_cnt1_ok) {
    auto timer = pico_framework_auto_timer_ms(
            "single_thread_framework_auto_timer_ms_time1_cnt1_ok");
    sleep(1);
}

TEST(AutoTimerTest, single_thread_framework_auto_timer_ms_time1_cnt2_ok) {
    for (int i = 0; i < 2; i++) {
        auto timer = pico_framework_auto_timer_ms(
                "single_thread_framework_auto_timer_ms_time1_cnt12_ok");
        sleep(1);
        timer.start();
        sleep(1);
        timer.stop();
    }
}

TEST(AutoTimerTest, multi_thread_framework_auto_timer_ms_time1_cnt2_ok) {
    ThreadGroup tg(10);
    std::vector<AsyncReturn> rets(tg.size());

    auto func = [&](int tid, int) {
        for (int i = 0; i < 2; i++) {
            auto timer = pico_framework_auto_timer_ms(format_string(
                    "multi_thread_framework_auto_timer_ms_time1_cnt2_thread_%d_ok", tid));
            sleep(1);
        }
    };
    
    for (size_t i = 0; i < rets.size(); ++i) {
        SCHECK(tg.async_exec(rets[i], [&func, i](int thrd_id){ func(thrd_id, i);}));
    }

    for (auto& ret : rets) {
        ret.wait();
    }
}

TEST(AutoTimerTest, single_thread_application_auto_timer_us_time1_cnt2_ok) {
    for (int i = 0; i < 2; i++) {
        auto timer = pico_application_auto_timer_us(
                "single_thread_framework_auto_timer_us_time1_cnt2_ok");
        sleep(1);
    }
}

TEST(AutoTimerTest, auto_timer_performance) {
    auto timer = pico_framework_auto_timer_us("auto_timer_performance_us_1M");
    for (int i = 0; i < 10000; i++) {
        PICO_AUTO_TIMER_FAST(tmp, "application", "dummy", PicoTime::us);
        PICO_AUTO_TIMER_FAST_V(tmp2, "application", "dummy2", PicoTime::us, 102400);
        tmp2.start();
        tmp.start();
        tmp.stop();
        tmp2.stop();
    }
}

TEST(AutoTimerTest, single_thread_application_vtimer_ok) {
    std::thread([]() {
        for (int i = 0; i < 10000; i++) {
            VTIMER(1, application, vtimer, us, 1024);
            VTIMER(1, application, vtimer2, us);
        }
    }).join();
}

}
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    paradigm4::pico::core::pico_is_evaluate_performance() = true;
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
