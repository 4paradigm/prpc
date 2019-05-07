#include <cstdio>
#include <cstdlib>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "Master.h"
#include "MasterClient.h"

#include "pico_test_common.h"
#include "pico_unittest_operator.h"
#include "common/include/PicoContext.h"
#include "common/include/initialize.h"

namespace paradigm4 {
namespace pico {

class ZkMasterClientTest : public ::testing::Test {
public:
    void SetUp() override {
        ASSERT_TRUE(pico_context().env_config().master.type == "zk");
        ZKMASTER zk_config = pico_context().env_config().master.zk;
        server_list = zk_config.endpoint;
        recv_timeout = zk_config.recv_timeout;
        disconnect_timeout = zk_config.disconnect_timeout;
        sub_id = 0;
    }

    void TearDown() override {
    }

    std::string gen_path() {
        int n = sub_id++;
        ZKMASTER zk_config = pico_context().env_config().master.zk;
        return zk_config.rootpath + "/" + std::to_string(n);
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

    client.add_rpc_name_id("api1", "key1", client.generate_id("api1"));
    client.get_rpc_name_id("api1", "key1", id);
    EXPECT_EQ(0, id);
    client.add_rpc_name_id("api1", "key2", client.generate_id("api1"));
    client.get_rpc_name_id("api1", "key2", id);
    EXPECT_EQ(1, id);
    client.add_rpc_name_id("api2", "key1", client.generate_id("api2"));
    client.get_rpc_name_id("api2", "key1", id);
    EXPECT_EQ(0, id);
    client.add_rpc_name_id("api1", "key2", 9);
    client.get_rpc_name_id("api1", "key2", id);
    EXPECT_EQ(1, id);
    client.add_rpc_name_id("api2", "key1", 9);
    client.get_rpc_name_id("api2", "key1", id);
    EXPECT_EQ(0, id);
    client.add_rpc_name_id("api2", "key2", client.generate_id("api2"));
    client.get_rpc_name_id("api2", "key2", id);
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
    info.role = PSERVER_ROLE_STR;
    info.global_rank = pserver1.generate_id(RANK_KEY);
    info.local_rank = pserver1.generate_id(PSERVER_ROLE_STR);
    info.endpoint = "127.0.0.1:1";
    pserver1.register_node(info);
    pserver1.set_node_ready(info.global_rank);
    EXPECT_EQ(0, info.global_rank);
    EXPECT_EQ(0, info.local_rank);

    ZkMasterClient pserver2(test_path, server_list, recv_timeout, disconnect_timeout);
    pserver2.initialize();
    info.global_rank = pserver2.generate_id(RANK_KEY);
    info.local_rank = pserver2.generate_id(PSERVER_ROLE_STR);
    info.endpoint = "127.0.0.1:2";
    pserver1.register_node(info);
    pserver1.set_node_ready(info.global_rank);
    EXPECT_EQ(1, info.global_rank);
    EXPECT_EQ(1, info.local_rank);

    ZkMasterClient client1(test_path, server_list, recv_timeout, disconnect_timeout);
    client1.initialize();
    info.role = LEARNER_ROLE_STR;
    info.global_rank = client1.generate_id(RANK_KEY);
    info.local_rank = client1.generate_id(LEARNER_ROLE_STR);
    info.endpoint = "127.0.0.1:101";
    client1.register_node(info);
    client1.set_node_ready(info.global_rank);
    EXPECT_EQ(2, info.global_rank);
    EXPECT_EQ(0, info.local_rank);

    ZkMasterClient client2(test_path, server_list, recv_timeout, disconnect_timeout);
    client2.initialize();
    info.global_rank = client2.generate_id(RANK_KEY);
    info.local_rank = client2.generate_id(LEARNER_ROLE_STR);
    info.endpoint = "127.0.0.1:102";
    client2.register_node(info);
    client2.set_node_ready(info.global_rank);
    EXPECT_EQ(3, info.global_rank);
    EXPECT_EQ(1, info.local_rank);

    ZkMasterClient client3(test_path, server_list, recv_timeout, disconnect_timeout);
    client3.initialize();
    info.global_rank = client3.generate_id(RANK_KEY);
    info.local_rank = client3.generate_id(LEARNER_ROLE_STR);
    info.endpoint = "127.0.0.1:103";
    client3.register_node(info);
    client3.set_node_ready(info.global_rank);
    EXPECT_EQ(4, info.global_rank);
    EXPECT_EQ(2, info.local_rank);


    auto pserver_list = client1.get_nodes(PSERVER_ROLE_STR);
    EXPECT_EQ(0, pserver_list[0]);
    EXPECT_EQ(1, pserver_list[1]);
    EXPECT_EQ(client1.get_comm_info(0).endpoint, "127.0.0.1:1");
    EXPECT_EQ(client1.get_comm_info(1).endpoint, "127.0.0.1:2");

    auto learner_list = client2.get_nodes(LEARNER_ROLE_STR);
    EXPECT_EQ(2, learner_list[0]);
    EXPECT_EQ(3, learner_list[1]);
    EXPECT_EQ(4, learner_list[2]);
    EXPECT_EQ(client2.get_comm_info(2).endpoint, "127.0.0.1:101");
    EXPECT_EQ(client2.get_comm_info(3).endpoint, "127.0.0.1:102");
    EXPECT_EQ(client2.get_comm_info(4).endpoint, "127.0.0.1:103");


    auto node_list = client3.get_nodes();
    EXPECT_EQ(0, node_list[0]);
    EXPECT_EQ(1, node_list[1]);
    EXPECT_EQ(2, node_list[2]);
    EXPECT_EQ(3, node_list[3]);
    EXPECT_EQ(4, node_list[4]);

    pserver1.set_node_finished(0);
    pserver1.finalize();
    pserver2.set_node_finished(1);
    pserver2.finalize();
    client1.set_node_finished(2);
    client1.finalize();
    client2.set_node_finished(3);
    client2.finalize();
    client3.set_node_finished(4);
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
    paradigm4::pico::test::PicoUnitTestCommon::singleton().initialize(&argc, argv);
    if (paradigm4::pico::test::PicoUnitTestOperator::singleton().is_show_operator()) {
        // local_wrapper, repeat_num=10
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(
              paradigm4::pico::test::LocalWrapperOperator(10, 0, "zk"));
        paradigm4::pico::test::PicoUnitTestOperator::singleton().show_operator();
        paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
        return 0;
    }
    paradigm4::pico::pico_initialize(argc, argv);
    int ret = RUN_ALL_TESTS();
    paradigm4::pico::pico_finalize();
    paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
    return ret;
}
