#include <memory>
#include <thread>
#include <atomic>
#include <algorithm>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "ChannelReplicator.h"

namespace paradigm4 {
namespace pico {
namespace core {

static size_t g_capacity = 30;
static int g_count = 200;
static size_t g_cache_size = 20;

void r_func(Channel<int> chan) {
    std::vector<int> read_data;
    for (int i = 0; i < g_count; ++i) {
        int val = 0;
        chan.read(val);
        read_data.push_back(val);
    }
    std::sort(read_data.begin(), read_data.end());
    for (int i = 0; i < g_count; ++i) {
        EXPECT_EQ(i, read_data[i]);
    }
}

TEST(ChannelReplicatorRunTwice, read_write_twice_ok) {
    ChannelReplicator<int> g_cr(g_capacity, g_cache_size);
    EXPECT_EQ(g_cr.chan_capacity(), g_capacity);
    EXPECT_EQ(g_cr.cache_size(), g_cache_size);


    auto g_out_chan1 = Channel<int>();
    auto g_out_chan2 = Channel<int>();
    auto g_out_chan3 = Channel<int>();
    auto g_out_chan4 = Channel<int>();

    {
        g_cr.add_output_chan(g_out_chan1);
        g_cr.add_output_chan(g_out_chan2);
        g_cr.add_output_chan(g_out_chan3);
        g_cr.add_output_chan(g_out_chan4);

        g_cr.initialize();

        std::thread t_w([&] {
            for (int i = 0; i < g_count; ++i) {
                g_cr.input_chan().write(i);
            }
        });
        std::thread t1(r_func, std::ref(g_out_chan1));
        std::thread t2(r_func, std::ref(g_out_chan2));
        std::thread t3(r_func, std::ref(g_out_chan3));
        std::thread t4(r_func, std::ref(g_out_chan4));
        t_w.join();
        t1.join();
        t2.join();
        t3.join();
        t4.join();

        g_cr.close();
        g_cr.join();
        g_cr.finalize();
    }
    {
        g_cr.add_output_chan(g_out_chan1);
        g_cr.add_output_chan(g_out_chan2);
        g_cr.add_output_chan(g_out_chan3);
        g_cr.add_output_chan(g_out_chan4);

        g_cr.initialize();

        std::thread t_w([&] {
            for (int i = 0; i < g_count; ++i) {
                g_cr.input_chan().write(i);
            }
        });
        std::thread t1(r_func, std::ref(g_out_chan1));
        std::thread t2(r_func, std::ref(g_out_chan2));
        std::thread t3(r_func, std::ref(g_out_chan3));
        std::thread t4(r_func, std::ref(g_out_chan4));
        t_w.join();
        t1.join();
        t2.join();
        t3.join();
        t4.join();

        g_cr.close();
        g_cr.join();
        g_cr.finalize();
    }
    EXPECT_EQ(g_cr.input_chan().size(), 0u);
    EXPECT_EQ(g_out_chan1.size(), 0u);
    EXPECT_EQ(g_out_chan2.size(), 0u);
    EXPECT_EQ(g_out_chan3.size(), 0u);
    EXPECT_EQ(g_out_chan4.size(), 0u);
}

TEST(ChannelReplicator, read_write_ok) {
    ChannelReplicator<int> g_cr(g_capacity, g_cache_size);
    EXPECT_EQ(g_cr.chan_capacity(), g_capacity);
    EXPECT_EQ(g_cr.cache_size(), g_cache_size);


    auto g_out_chan1 = Channel<int>();
    auto g_out_chan2 = Channel<int>();
    auto g_out_chan3 = Channel<int>();
    auto g_out_chan4 = Channel<int>();

    g_cr.add_output_chan(g_out_chan1);
    g_cr.add_output_chan(g_out_chan2);
    g_cr.add_output_chan(g_out_chan3);
    g_cr.add_output_chan(g_out_chan4);

    g_cr.initialize();

    std::thread t_w([&] {
        for (int i = 0; i < g_count; ++i) {
            g_cr.input_chan().write(i);
        }
    });
    std::thread t1(r_func, std::ref(g_out_chan1));
    std::thread t2(r_func, std::ref(g_out_chan2));
    std::thread t3(r_func, std::ref(g_out_chan3));
    std::thread t4(r_func, std::ref(g_out_chan4));
    t_w.join();
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    g_cr.close();
    g_cr.join();
    g_cr.finalize();
    EXPECT_EQ(g_cr.input_chan().size(), 0u);
    EXPECT_EQ(g_out_chan1.size(), 0u);
    EXPECT_EQ(g_out_chan2.size(), 0u);
    EXPECT_EQ(g_out_chan3.size(), 0u);
    EXPECT_EQ(g_out_chan4.size(), 0u);
}

TEST(ChannelReplicator, no_output_chan_ok) {
    ChannelReplicator<int> g_cr(g_capacity, g_cache_size);
    g_cr.initialize();

    std::thread t_w([&] {
        for (int i = 0; i < g_count; ++i) {
            g_cr.input_chan().write(i);
        }
    });

    t_w.join();
    g_cr.close();
    g_cr.join();
    EXPECT_EQ(g_cr.input_chan().size(), 0u);
}

TEST(ChannelReplicator, one_output_chan_ok) {
    ChannelReplicator<int> g_cr(g_capacity, g_cache_size);
    auto g_out_chan = Channel<int>();

    g_cr.add_output_chan(g_out_chan);

    g_cr.initialize();

    std::thread t_w([&] {
        for (int i = 0; i < g_count; ++i) {
            g_cr.input_chan().write(i);
        }
    });


    std::thread t_r([&] {
        int val = 0;
        std::vector<int> read_data;
        for (int i = 0; i < g_count; ++i) {
            g_out_chan.read(val);
            read_data.push_back(val);
        }
        std::sort(read_data.begin(), read_data.end());
        for (int i = 0; i < g_count; ++i) {
            EXPECT_EQ(i, read_data[i]);
        }
    });

    t_w.join();
    t_r.join();
    g_cr.close();
    g_cr.join();
    EXPECT_EQ(g_cr.input_chan().size(), 0u);
}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

