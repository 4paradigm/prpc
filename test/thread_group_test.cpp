#include <memory>
#include <thread>
#include <atomic>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "gflags/gflags.h"

#include "Channel.h"
#include "pico_test_common.h"
#include "pico_unittest_operator.h"
#include "ThreadGroup.h"

namespace paradigm4 {
namespace pico {

static const size_t g_capacity = 100;

TEST(ThreadGroup, exec_ok) {
    {
        ThreadGroup tg;
        auto g_int_chan = Channel<int>(g_capacity);
        auto g_int_chan2 = g_int_chan;
        EXPECT_EQ(0u, g_int_chan.size());
        auto ret1 = tg.async_exec([&](int) {g_int_chan.write(1);});
        auto ret2 =  tg.async_exec([&](int) {g_int_chan2.write(2);});
        ret1.wait();
        ret2.wait();
        EXPECT_EQ(2u, g_int_chan.size());
        EXPECT_EQ(2u, g_int_chan2.size());
        int i = 0;
        EXPECT_TRUE(g_int_chan.read(i));
        EXPECT_TRUE(g_int_chan2.read(i));
        EXPECT_EQ(0u, g_int_chan.size());
        EXPECT_EQ(0u, g_int_chan2.size());
    }
    {
        ThreadGroup tg(2);
        auto g_int_chan = Channel<int>(10);
        auto f1 = [&](int) {
            int i = 0;
            int expect_i = 0;

            while (g_int_chan.read(i)) {
                EXPECT_EQ(expect_i++, i);
            }
        };
        auto f2 = [&](int) {
            int e_array[g_capacity];

            for (size_t i = 0; i < g_capacity; ++i) {
                e_array[i] = i;
            }

            g_int_chan.write(e_array, g_capacity);
            g_int_chan.close();
        };
        auto ret1 = tg.async_exec(f1);
        auto ret2 = tg.async_exec(f2);
        ret1.wait();
        ret2.wait();
        int i = 0;
        EXPECT_FALSE(g_int_chan.read(i));
        EXPECT_FALSE(g_int_chan.read(i));
    }
    {
        auto g_int_chan = Channel<int>(10);
        std::atomic<size_t> read_count(0);
        auto f1 = [&](int) {
            int i = 0;

            while (g_int_chan.read(i)) {
                EXPECT_GT(static_cast<int>(g_capacity), i);
                read_count++;

                if (i > 50) {
                    break;
                }
            }
        };
        auto f2 = [&](int) {
            int i = 0;

            while (g_int_chan.read(i)) {
                EXPECT_GT(static_cast<int>(g_capacity), i);
                read_count++;
            }
        };
        auto f3 = [&](int) {
            int e_array[50];

            for (size_t i = 0; i < 50; ++i) {
                e_array[i] = i;
            }

            EXPECT_EQ(g_int_chan.write(e_array, 50), 50u);
        };
        auto f4 = [&](int) {
            int e_array[50];

            for (size_t i = 0; i < 50; ++i) {
                e_array[i] = i + 50;
            }

            EXPECT_EQ(g_int_chan.write(e_array, 50), 50u);
        };

        ThreadGroup tg(4);
        auto ret1 = tg.async_exec(f1);
        auto ret2 = tg.async_exec(f2);
        auto ret3 = tg.async_exec(f3);
        auto ret4 = tg.async_exec(f4);

        ret3.wait();
        ret4.wait();
        g_int_chan.close();
        ret1.wait();
        ret2.wait();

        int main_i = 0;
        if (g_int_chan.read(main_i)) {
            read_count++;
        }

        EXPECT_EQ(g_capacity, read_count);
    }
}

TEST(ThreadGroup, parallel_run_ok) {
    size_t size = pico_concurrency();
    std::vector<int> sum(size, 0);
    size_t count = 10;

    pico_parallel_run([&sum, &count](int, int id) {
        for (size_t i = 0; i < count; ++i) {
            sum[id] += id + i;
        }
    });

    for (size_t i = 0; i < sum.size(); ++i) {
        EXPECT_EQ(sum[i], static_cast<int>(i * count + count * (count - 1) / 2));
    }
}

TEST(ThreadGroup, parallel_run_range_ok) {
    size_t size = pico_concurrency();
    std::vector<int> sum(size, 0);
    size_t count = 1000;

    pico_parallel_run_range(count, [&sum](int, int id, size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            sum[id] += i;
        }
    });

    size_t total_count = 0;
    for (auto& e : sum) {
        total_count += e;
    }
    EXPECT_EQ(total_count, count * (count - 1) / 2);
}

TEST(ThreadGroup, parallel_run_dynamic_ok) {
    size_t pair_num = 100;
    size_t width = 10;
    std::vector<std::pair<size_t, size_t>> bes;
    for (size_t i = 0 ; i < pair_num; ++i) {
        bes.push_back({i * width, (i + 1) * width});
    }

    std::vector<size_t> sum(pair_num);

    pico_parallel_run_dynamic(pair_num, [&bes, &sum](int, int id) {
        for (size_t i = bes[id].first; i < bes[id].second; ++i) {
            sum[id] += i;
        }
    });

    size_t total_count = 0;
    for (auto& e : sum) {
        total_count += e;
    }
    EXPECT_EQ(total_count, width * pair_num * (width * pair_num - 1) / 2);
}

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    paradigm4::pico::test::PicoUnitTestCommon::singleton().initialize(&argc, argv);
    if (paradigm4::pico::test::PicoUnitTestOperator::singleton().is_show_operator()) {
        // no_wrapper, repeat_num=5
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(paradigm4::pico::test::NoWrapperOperator(5));
        paradigm4::pico::test::PicoUnitTestOperator::singleton().show_operator();
        paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
        return 0;
    }
    paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
    return RUN_ALL_TESTS();
}

