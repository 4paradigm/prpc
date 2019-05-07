#include <cstdio>
#include <cstdlib>

#include <glog/logging.h>
#include <gtest/gtest.h>
#include <iostream>

#include "Master.h"
#include "MasterClient.h"

#include "pico_test_common.h"
#include "pico_unittest_operator.h"

namespace paradigm4 {
namespace pico {



TEST(MasterTest, Init) {
    Master master("127.0.0.1");
    master.initialize();
    SLOG(INFO) << "Master initialized.";
    TcpMasterClient client(master.endpoint());
    client.initialize();
    SLOG(INFO) << "Client initialized.";
    client.clear_master();
    client.finalize();
    master.exit();
    master.finalize();
}

TEST(MasterTest, Lock) {
    Master master("127.0.0.1");
    master.initialize();
    TcpMasterClient client(master.endpoint());
    client.initialize();
    client.acquire_lock("lock1");
    auto func = [&]() {
        TcpMasterClient client(master.endpoint());
        client.initialize();
        client.acquire_lock("lock1");
        client.release_lock("lock1");
        client.finalize();
    };
    std::thread th(func);
    client.release_lock("lock1");
    th.join();
    client.acquire_lock("lock2");
    client.release_lock("lock2");
    client.clear_master();
    client.finalize();
    master.exit();
    master.finalize();
}


TEST(MasterTest, MTLock) {
    Master master("127.0.0.1");
    master.initialize();
    TcpMasterClient client(master.endpoint());
    client.initialize();
    auto func = [&]() {
        for (int i = 0; i < 10; ++i) {
            client.acquire_lock("lock1");
            client.acquire_lock("lock2");
            client.release_lock("lock2");
            client.release_lock("lock1");
        }
    };
    std::vector<std::thread> ths(4);
    for (auto& th : ths) {
        th = std::thread(func);
    }
    for (auto& th : ths) {
        th.join();
    }
    client.clear_master();
    client.finalize();
    master.exit();
    master.finalize();
}

TEST(MasterTest, Barrier) {
    Master master("127.0.0.1");
    master.initialize();
    auto barrier_test = [&](const std::string& barrier_name, size_t n) {
        SLOG(INFO) << "barrer name: " << barrier_name
                   << " with node number: " << n << " started";
        auto func = [&](const std::string& barrier_name, size_t n) {
            TcpMasterClient client(master.endpoint());
            client.initialize();
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
        SLOG(INFO) << "barrer name: " << barrier_name
                   << " with node number: " << n << " finished";
    };
    barrier_test("b1", 5);
    barrier_test("b1", 10);
    barrier_test("b2", 10);
    barrier_test("b1", 5);
    barrier_test("b2", 5);

    master.exit();
    master.finalize();
}

TEST(MasterTest, GenerateID) {
    Master master("127.0.0.1");
    master.initialize();
    TcpMasterClient client(master.endpoint());
    client.initialize();
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
    master.exit();
    master.finalize();
}

TEST(MasterTest, RegisterNode) {
    Master master("127.0.0.1");
    master.initialize();

    TcpMasterClient pserver1(master.endpoint());
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

    TcpMasterClient pserver2(master.endpoint());
    pserver2.initialize();
    info.global_rank = pserver2.generate_id(RANK_KEY);
    info.local_rank = pserver2.generate_id(PSERVER_ROLE_STR);
    info.endpoint = "127.0.0.1:2";
    pserver1.register_node(info);
    pserver1.set_node_ready(info.global_rank);
    EXPECT_EQ(1, info.global_rank);
    EXPECT_EQ(1, info.local_rank);

    TcpMasterClient client1(master.endpoint());
    client1.initialize();
    info.role = LEARNER_ROLE_STR;
    info.global_rank = client1.generate_id(RANK_KEY);
    info.local_rank = client1.generate_id(LEARNER_ROLE_STR);
    info.endpoint = "127.0.0.1:101";
    client1.register_node(info);
    client1.set_node_ready(info.global_rank);
    EXPECT_EQ(2, info.global_rank);
    EXPECT_EQ(0, info.local_rank);

    TcpMasterClient client2(master.endpoint());
    client2.initialize();
    info.global_rank = client2.generate_id(RANK_KEY);
    info.local_rank = client2.generate_id(LEARNER_ROLE_STR);
    info.endpoint = "127.0.0.1:102";
    client2.register_node(info);
    client2.set_node_ready(info.global_rank);
    EXPECT_EQ(3, info.global_rank);
    EXPECT_EQ(1, info.local_rank);

    TcpMasterClient client3(master.endpoint());
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
    master.exit();
    master.finalize();
}

TEST(MasterTest, Ctx) {
    Master master("127.0.0.1");
    master.initialize();
    TcpMasterClient client(master.endpoint());
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
    master.exit();
    master.finalize();
}

TEST(MasterTest, Snapshot) {
    Master master("127.0.0.1");
    master.initialize();
    TcpMasterClient client(master.endpoint());
    client.initialize();

    TcpMasterClient client1(master.endpoint());
    client1.initialize();
    client1.register_node({0, "127.0.0.1:1", "Learner", 0});
    client1.set_node_ready(0);

    TcpMasterClient client2(master.endpoint());
    client2.initialize();
    client2.register_node({1, "127.0.0.1:2", "Learner", 1});
    client1.set_node_ready(1);
    
    TcpMasterClient client3(master.endpoint());
    client3.initialize();
    client3.register_node({2, "127.0.0.1:3", "Learner", 2});
    client1.set_node_ready(2);
    
    PicoJsonNode snapshot; 
    EXPECT_TRUE(client.get_snapshot(snapshot));
    EXPECT_TRUE(snapshot.has("nodes"));
    EXPECT_EQ(snapshot.at("nodes").size(), 3);
    std::map<comm_rank_t, CommInfo> nodes;
    auto& nodes_json = snapshot.at("nodes");
    for (auto it = nodes_json.begin(); it != nodes_json.end(); it++) {
        CommInfo node;
        node.from_json_node(*it);
        nodes.emplace(node.global_rank, node);
    }
    for (int i = 0; i < 3; i++) {
        auto it = nodes.find(i);
        EXPECT_TRUE(it != nodes.end());
        EXPECT_EQ(it->second.endpoint, "127.0.0.1:" + std::to_string(i + 1));
        EXPECT_EQ(it->second.role, "Learner");
    }


    client3.set_node_finished(2);
    client3.finalize();
    client2.set_node_finished(1);
    client2.finalize();
    client1.set_node_finished(0);
    client1.finalize();

    client.clear_master();
    client.finalize();
    master.exit();
    master.finalize();
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

    int ret = RUN_ALL_TESTS();
    return ret;
}
