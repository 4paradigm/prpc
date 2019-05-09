#include <cstdio>
#include <cstdlib>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "Master.h"
#include "MasterClient.h"


namespace paradigm4 {
namespace pico {

DEFINE_string(endpoint, "", "");
DEFINE_string(rootpath, "", "");
DEFINE_uint64(recv_timeout, 0, "");
DEFINE_uint64(disconnect_timeout, 0, "");
const std::string RANK_KEY="asdf";

class ZkMasterClientTest : public ::testing::Test {
public:
    void SetUp() override {
        server_list = FLAGS_endpoint;
        recv_timeout = FLAGS_recv_timeout;
        disconnect_timeout = FLAGS_disconnect_timeout;
        root_path = FLAGS_rootpath;
        sub_id = 0;
    }

    void TearDown() override {
    }

    std::string gen_path() {
        int n = sub_id++;
        return root_path + "/" + std::to_string(n);
    }

protected:
    std::string server_list;
    int recv_timeout;
    int disconnect_timeout;
    std::string root_path;
    std::atomic_int sub_id;
};

class MasterTest : public ZkMasterClientTest {
};

TEST_F(ZkMasterClientTest, GenerateID) {
    std::string test_path = gen_path();
    ZkMasterClient client(test_path, server_list, recv_timeout, disconnect_timeout);
    ASSERT_TRUE(client.initialize());
    int32_t id = client.generate_id("test_key1");
    EXPECT_EQ(0, id);
    id = client.generate_id("test_key1");
    EXPECT_EQ(1, id);
    id = client.generate_id("test_key1");
    EXPECT_EQ(2, id);
    id = client.generate_id("test_key2");
    EXPECT_EQ(0, id);
    id = client.generate_id("test_key2");
    EXPECT_EQ(1, id);
    client.clear_master();
    client.finalize();
}

TEST_F(ZkMasterClientTest, Lock) {
    std::string test_path = gen_path();
    ZkMasterClient client(test_path, server_list, recv_timeout, disconnect_timeout);
    ASSERT_TRUE(client.initialize());
    client.acquire_lock("lock1");
    auto func = [&]() {
        ZkMasterClient client2(test_path, server_list, recv_timeout, disconnect_timeout);
        ASSERT_TRUE(client2.initialize());
        client2.acquire_lock("lock1");
        client2.release_lock("lock1");
        client2.finalize();
    };
    std::thread th(func);
    client.release_lock("lock1");
    th.join();
    client.acquire_lock("lock2");
    client.release_lock("lock2");
    client.clear_master();
    client.finalize();
}

TEST_F(ZkMasterClientTest, MultiThreadLock) {
    std::string test_path = gen_path();
    ZkMasterClient client(test_path, server_list, recv_timeout, disconnect_timeout);
    ASSERT_TRUE(client.initialize());
    client.acquire_lock("lock1");
    int a = 0;
    auto func = [&]() {
        client.acquire_lock("lock1");
        for (int i = 0; i < 100; i++,a++) {}
        client.acquire_lock("lock2");
        for (int i = 0; i < 100; i++,a++) {}
        client.release_lock("lock2");
        client.release_lock("lock1");
    };

    std::vector<std::thread> th(5);
    for (size_t i = 0; i < th.size(); ++i) {
        th[i] = std::thread(func);
    }
    for (int i = 0; i < 100; i++,a++) {}
    client.release_lock("lock1");
    for (size_t i = 0; i < th.size(); ++i) {
        th[i].join();
    }
    client.acquire_lock("lock2");
    client.release_lock("lock2");
    ASSERT_EQ(a, th.size() * 200 + 100);
    client.clear_master();
    client.finalize();
}

TEST_F(ZkMasterClientTest, Barrier) {
    std::string test_path = gen_path();
    ZkMasterClient controller(test_path, server_list, recv_timeout, disconnect_timeout);
    ASSERT_TRUE(controller.initialize());
    auto barrier_test = [&](const std::string& barrier_name, size_t n) {
        SLOG(INFO) << "barrer name: " << barrier_name << " with node number: " << n
                   << " started";
        auto func = [&](const std::string& barrier_name, size_t n) {
            ZkMasterClient client(test_path, server_list, recv_timeout, disconnect_timeout);
            ASSERT_TRUE(client.initialize());
            client.barrier(barrier_name, n);
            client.finalize();
        };
        std::vector<std::thread> th(n);
        for (size_t i = 0; i < n; ++i) {
            th[i] = std::thread(func, barrier_name, n);
        }
        for (size_t i = 0; i < n; ++i) {
            th[i].join();
        }
        SLOG(INFO) << "barrer name: " << barrier_name << " with node number: " << n
                   << " finished";
    };
    barrier_test("b1", 5);
    barrier_test("b1", 10);
    barrier_test("b2", 10);
    barrier_test("b1", 5);
    barrier_test("b2", 5);
    controller.clear_master();
    controller.finalize();
}

TEST_F(ZkMasterClientTest, MultiThreadBarrier) {
    std::string test_path = gen_path();
    ZkMasterClient client(test_path, server_list, recv_timeout, disconnect_timeout);
    ASSERT_TRUE(client.initialize());
    int barrier_num = 5;
    auto func = [&]() {
        client.barrier("a", barrier_num);
        client.barrier("a", barrier_num);
        client.barrier("b", barrier_num);
        client.barrier("a", barrier_num);
    };
    std::vector<std::thread> threads(barrier_num);
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i] = std::thread(func);
    }
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }
    client.clear_master();
    client.finalize();

}

TEST_F(ZkMasterClientTest, RegisterNode) {
    std::string test_path = gen_path();
    
    ZkMasterClient pserver1(test_path, server_list, recv_timeout, disconnect_timeout);
    pserver1.initialize();
    CommInfo info;
    info.global_rank = pserver1.generate_id(RANK_KEY);
    info.endpoint = "127.0.0.1:1";
    pserver1.register_node(info);
    EXPECT_EQ(0, info.global_rank);

    ZkMasterClient pserver2(test_path, server_list, recv_timeout, disconnect_timeout);
    pserver2.initialize();
    info.global_rank = pserver2.generate_id(RANK_KEY);
    info.endpoint = "127.0.0.1:2";
    pserver1.register_node(info);
    EXPECT_EQ(1, info.global_rank);

    ZkMasterClient client1(test_path, server_list, recv_timeout, disconnect_timeout);
    client1.initialize();
    info.global_rank = client1.generate_id(RANK_KEY);
    info.endpoint = "127.0.0.1:101";
    client1.register_node(info);
    EXPECT_EQ(2, info.global_rank);

    ZkMasterClient client2(test_path, server_list, recv_timeout, disconnect_timeout);
    client2.initialize();
    info.global_rank = client2.generate_id(RANK_KEY);
    info.endpoint = "127.0.0.1:102";
    client2.register_node(info);
    EXPECT_EQ(3, info.global_rank);

    ZkMasterClient client3(test_path, server_list, recv_timeout, disconnect_timeout);
    client3.initialize();
    info.global_rank = client3.generate_id(RANK_KEY);
    info.endpoint = "127.0.0.1:103";
    client3.register_node(info);
    EXPECT_EQ(4, info.global_rank);



    std::vector<CommInfo> nodes;
    ASSERT_TRUE(client3.get_comm_info(nodes));
    EXPECT_EQ(0, nodes[0].global_rank);
    EXPECT_EQ(1, nodes[1].global_rank);
    EXPECT_EQ(2, nodes[2].global_rank);
    EXPECT_EQ(3, nodes[3].global_rank);
    EXPECT_EQ(4, nodes[4].global_rank);

    pserver1.finalize();
    pserver2.finalize();
    client1.finalize();
    client2.finalize();
    client3.clear_master();
    client3.finalize();
}

TEST_F(MasterTest, Ctx) {
    std::string test_path = gen_path();
    ZkMasterClient client(test_path, server_list, recv_timeout, disconnect_timeout);
    ASSERT_TRUE(client.initialize());

    client.initialize();

    SCHECK(client.add_context(0, "abc"));
    std::string str;
    bool ret = client.get_context(0, str);
    EXPECT_EQ(ret, true);
    EXPECT_EQ(str, "abc");

    SCHECK(client.add_context(1, "bcd"));
    ret = client.get_context(1, str);
    EXPECT_EQ(ret, true);
    EXPECT_EQ(str, "bcd");

    client.set_context(0, "def");
    ret = client.get_context(0, str);
    EXPECT_EQ(ret, true);
    EXPECT_EQ(str, "def");

    ret = client.get_context(2, str);
    EXPECT_EQ(ret, false);

    client.delete_storage(0);
    ret = client.get_context(0, str);
    EXPECT_EQ(ret, false);
    
    client.clear_master();
    client.finalize();
}

TEST_F(MasterTest, Model) {
    std::string test_path = gen_path();
    ZkMasterClient client(test_path, server_list, recv_timeout, disconnect_timeout);
    ASSERT_TRUE(client.initialize());

    std::string model_name = "test";
    client.add_model(model_name, "abc");
    std::string changed_model = "bcd";
    int called = 0;
    std::mutex mtx;
    std::condition_variable cond;
    auto func = [&]() {
        {
            std::unique_lock<std::mutex> lock(mtx);
            ++called;
        }
        cond.notify_one();
    };
    auto h = client.watch_model("test", func);

    client.set_model(model_name, changed_model);
    {
        std::unique_lock<std::mutex> lock(mtx);
        cond.wait(lock, [&]() {return called == 1;});
    }
    EXPECT_EQ(called, 1);
    changed_model = "defg";

    client.set_model(model_name, changed_model);
    {
        std::unique_lock<std::mutex> lock(mtx);
        cond.wait(lock, [&]() {return called == 2;});
    }
    EXPECT_EQ(called, 2);

    client.cancle_watch(h);
    client.clear_master();
    client.finalize();
}

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
