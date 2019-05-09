#include "RpcChannel.h"
#include <iostream>
#include <thread>
#include "glog/logging.h"
#include "gtest/gtest.h"
#include "gflags/gflags.h"

namespace paradigm4 {
namespace pico {

using namespace std;

const int N = 10;
TEST(RpcChannel, read_write_ok) {
    RpcChannel<int> ch1;
    RpcChannel<int> ch2;
    std::thread t1 = std::thread([&]() {
        for (int i = 0; i < N; ++i) {
            int x = N;
            ch1.send(std::move(i));
            ch2.recv(x, -1);
            CHECK(i == x);
        }
    });

    std::thread t2 = std::thread([&]() {
        int x, cnt = 0;
        while (ch1.recv(x, -1)) {
            ++cnt;
            ch2.send(std::move(x));
        }
        CHECK(cnt == N);
    });
    t1.join();
    ch1.terminate();
    ch2.terminate();
    t2.join();
}

}
}


int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

