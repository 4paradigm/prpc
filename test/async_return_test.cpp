#include <cstdlib>
#include <cstdio>
#include <thread>
#include <chrono>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "Channel.h"
#include "AsyncReturn.h"

namespace paradigm4 {
namespace pico {

TEST(AsyncReturn, execute_ok) {
    typedef std::pair<std::string, AsyncReturn> TaskPair;
    Channel<TaskPair> chan;
    std::string g_str = "NONE";
    auto agent_run = [&chan, &g_str]() {
        TaskPair tp;
        while(chan.read(tp)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            g_str = tp.first;
            tp.second.notify();
        }
    };

    std::thread agent = std::thread(agent_run);
    
    auto caller = [&chan](const std::string& name) -> AsyncReturn {
        AsyncReturn ret;
        chan.write({name, ret});
        return ret;
    };

    EXPECT_STREQ(g_str.c_str(), "NONE");

    AsyncReturn ret = caller("Pico");
    ret.wait();

    EXPECT_STREQ(g_str.c_str(), "Pico");

    chan.close();
    agent.join();
}


TEST(AsyncReturn, template_int_execute_ok) {
    typedef std::pair<std::string, AsyncReturnV<int>> TaskPair;
    Channel<TaskPair> chan;
    std::string g_str = "NONE";
    auto agent_run = [&chan, &g_str]() {
        TaskPair tp;
        while(chan.read(tp)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            g_str = tp.first;
            tp.second.notify(std::make_shared<int>(3));
        }
    };

    std::thread agent = std::thread(agent_run);
    
    auto caller = [&chan](const std::string& name) -> AsyncReturnV<int> {
        AsyncReturnV<int> ret;
        chan.write({name, ret});
        return ret;
    };

    EXPECT_STREQ(g_str.c_str(), "NONE");

    AsyncReturnV<int> ret = caller("Pico");
    auto t = ret.wait();
    EXPECT_TRUE(*t == 3);

    EXPECT_STREQ(g_str.c_str(), "Pico");

    chan.close();
    agent.join();
}

TEST(AsyncReturn, template_string_execute_ok) {
    typedef std::pair<std::string, AsyncReturnV<std::string>> TaskPair;
    Channel<TaskPair> chan;
    std::string g_str = "NONE";
    auto agent_run = [&chan, &g_str]() {
        TaskPair tp;
        while(chan.read(tp)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            g_str = tp.first;
            tp.second.notify(std::make_shared<std::string>("pico pico"));
        }
    };

    std::thread agent = std::thread(agent_run);
    
    auto caller = [&chan](const std::string& name) -> AsyncReturnV<std::string> {
        AsyncReturnV<std::string> ret;
        chan.write({name, ret});
        return ret;
    };

    EXPECT_STREQ(g_str.c_str(), "NONE");

    AsyncReturnV<std::string> ret = caller("Pico");
    auto t = ret.wait();
    EXPECT_TRUE(*t == "pico pico");

    EXPECT_STREQ(g_str.c_str(), "Pico");

    chan.close();
    agent.join();
}

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
