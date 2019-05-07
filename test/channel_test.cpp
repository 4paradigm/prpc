#include <memory>
#include <thread>
#include <atomic>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "gflags/gflags.h"
#include "pico_test_common.h"
#include "pico_unittest_operator.h"
#include "Channel.h"

namespace paradigm4 {
namespace pico {

static const size_t g_capacity = 100;

TEST(Channel, read_write_ok) {
    {
        auto g_int_chan = Channel<int>(g_capacity);
        auto g_int_chan2 = g_int_chan;
        EXPECT_EQ(0u, g_int_chan.size());
        std::thread t1([&] {g_int_chan.write(1);});
        std::thread t2([&] {g_int_chan2.write(2);});
        t1.join();
        t2.join();
        EXPECT_EQ(2u, g_int_chan.size());
        EXPECT_EQ(2u, g_int_chan2.size());
        int i = 0;
        EXPECT_TRUE(g_int_chan.read(i));
        EXPECT_TRUE(g_int_chan2.read(i));
        EXPECT_FALSE(g_int_chan.try_read(i));
        EXPECT_FALSE(g_int_chan2.try_read(i));
        EXPECT_EQ(0u, g_int_chan.size());
        EXPECT_EQ(0u, g_int_chan2.size());
    }
    {
        auto g_int_chan = Channel<int>(g_capacity);
        EXPECT_EQ(0u, g_int_chan.size());
        int e_array1[g_capacity];

        for (size_t i = 0; i < g_capacity; ++i) {
            e_array1[i] = i;
        }

        g_int_chan.write(e_array1, g_capacity);
        EXPECT_EQ(g_capacity, g_int_chan.size());
        int e_array2[g_capacity];
        EXPECT_TRUE(g_int_chan.read(e_array2, g_capacity));

        for (size_t i = 0; i < g_capacity; ++i) {
            EXPECT_EQ(e_array1[i], e_array2[i]);
        }

        EXPECT_EQ(0u, g_int_chan.size());
    }
    {
        auto g_int_chan = Channel<int>(g_capacity);
        EXPECT_TRUE(g_int_chan.try_write(10));

        g_int_chan.close();

        int i = 0;
        EXPECT_TRUE(g_int_chan.try_read(i));
        EXPECT_EQ(i, 10);

        EXPECT_FALSE(g_int_chan.try_read(i));
        EXPECT_EQ(0u, g_int_chan.size());
    }
}

TEST(Channel, loop_ok) {
    {
        auto g_int_chan = Channel<int>(10);
        auto f1 = [&]() {
            int i = 0;
            int expect_i = 0;

            while (g_int_chan.read(i)) {
                EXPECT_EQ(expect_i++, i);
            }
        };
        auto f2 = [&]() {
            int e_array[g_capacity];

            for (size_t i = 0; i < g_capacity; ++i) {
                e_array[i] = i;
            }

            g_int_chan.write(e_array, g_capacity);
            g_int_chan.close();
        };
        std::thread t1(f1);
        std::thread t2(f2);
        t1.join();
        t2.join();
        int i = 0;
        EXPECT_FALSE(g_int_chan.read(i));
        EXPECT_FALSE(g_int_chan.read(i));
    }
    {
        auto g_int_chan = Channel<int>(10u);
        std::atomic<size_t> read_count(0u);
        auto f1 = [&]() {
            int i = 0;

            while (g_int_chan.read(i)) {
                EXPECT_GT(static_cast<int>(g_capacity), i);
                read_count++;

                if (i > 50) {
                    break;
                }
            }
        };
        auto f2 = [&]() {
            int i = 0;

            while (g_int_chan.read(i)) {
                EXPECT_GT(static_cast<int>(g_capacity), i);
                read_count++;
            }
        };
        auto f3 = [&]() {
            int e_array[50];

            for (size_t i = 0; i < 50; ++i) {
                e_array[i] = i;
            }

            EXPECT_EQ(g_int_chan.write(e_array, 50), 50u);
        };
        auto f4 = [&]() {
            int e_array[50];

            for (size_t i = 0; i < 50; ++i) {
                e_array[i] = i + 50;
            }

            EXPECT_EQ(g_int_chan.write(e_array, 50), 50u);
        };

        auto t1 = std::thread(f1);
        auto t2 = std::thread(f2);
        auto t3 = std::thread(f3);
        auto t4 = std::thread(f4);

        t3.join();
        t4.join();
        g_int_chan.close();
        t1.join();
        t2.join();

        int main_i = 0;
        if (g_int_chan.read(main_i)) {
            read_count++;
        }

        EXPECT_EQ(g_capacity, read_count);
    }
}

template<size_t size = 1024>
struct Blob {
    int a[size];
    operator int() {
        return a[0];
    }
    Blob(const int i = 0) {
        a[0] = i;
    }
    template<size_t other_size>
    Blob(Blob<other_size> other) {
        a[0] = other.a[0];
    }
    bool operator==(const Blob& other) {
        return a[0] == other.a[0];
    }
    friend std::ostream& operator<<(std::ostream& out, const Blob& b) {
        out << (b.a[0]);
        return out;
    }
};

TEST(Channel, oom) {
    paradigm4::pico::pico_mem().initialize();
    size_t cap = 1024 * 1024;
    const size_t chan_a_size = 1024;
    const size_t chan_b_size = 2048;
    typedef std::chrono::milliseconds DurationMs;
    auto chan_a = Channel<Blob<chan_a_size>>(cap);
    auto chan_b = Channel<Blob<chan_b_size>>(cap);
    auto f0 = [&]() {
        int i = 0;
        while (i < 10000) {
            chan_a.write(i++);
            if (i % 100 == 0) {
                LOG(INFO) << "chan_a write: " << i;
                std::this_thread::sleep_for(DurationMs(1));
            }
        }
        chan_a.close();
    };
    auto f1 = [&]() {
        Blob<chan_a_size> i = 0;
        int expect_i = 0;
        while (chan_a.read(i)) {
            if (i % 100 == 0)
                LOG(INFO) << "chan_a read: " << i;
            EXPECT_EQ(expect_i++, int(i));
            chan_b.write(i);
            if (i % 100 == 0)
                LOG(INFO) << "chan_b write: " << i;
        }
        chan_b.close();
    };
    auto f2 = [&]() {
        Blob<chan_b_size> i = 0;
        int expect_i = 0;
        while (chan_b.read(i)) {
            if (i % 100 == 0) {
                LOG(INFO) << "chan_b read: " << i;
                std::this_thread::sleep_for(DurationMs(5));
            }
            EXPECT_EQ(expect_i++, int(i));
        }
    };
    std::thread t0(f0);
    std::thread t1(f1);
    std::thread t2(f2);
    t0.join();
    t1.join();
    t2.join();
    LOG(INFO) << "finished ";
}

TEST(Channel, shared_open_ok) {
    Channel<int> g_int_chan;
    g_int_chan.shared_open();

    g_int_chan.close();
    EXPECT_TRUE(g_int_chan.closed());

    g_int_chan.shared_open();
    EXPECT_FALSE(g_int_chan.closed());

    g_int_chan.shared_open();

    g_int_chan.close();
    EXPECT_FALSE(g_int_chan.closed());

    g_int_chan.close();
    EXPECT_TRUE(g_int_chan.closed());
}

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    paradigm4::pico::test::PicoUnitTestCommon::singleton().initialize(&argc, argv);
    if (paradigm4::pico::test::PicoUnitTestOperator::singleton().is_show_operator()) {
        // no_wrapper, repeat_num=100
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(paradigm4::pico::test::NoWrapperOperator(100));
        paradigm4::pico::test::PicoUnitTestOperator::singleton().show_operator();
        paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
        return 0;
    }
    paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
    return RUN_ALL_TESTS();
}

