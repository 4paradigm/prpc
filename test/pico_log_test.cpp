#include <cstdlib>
#include <cstdio>
#include <queue>
#include <string>

#include <glog/logging.h>
#include <gtest/gtest.h>
#include <thread>

#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

TEST(picoLog, check_ok) {
    SLOG(INFO) << "hello world\"'!@#$%^&*()_+";
    BLOG(2) << "debug log";
}

TEST(picoLog, multithread_ok) {
    std::vector<std::thread> rets(100);

    for (int i = 0; i < 100; i++) {
        auto func = [i](int tid) {
            SLOG(INFO) << "hello world " << i << " " << tid;
        };
        rets[i] = std::thread(func, i);
    }
    for (int i = 0; i < 100; i++) {
        rets[i].join();
    }
}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
