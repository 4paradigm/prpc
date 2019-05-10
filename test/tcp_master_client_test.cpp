#include <cstdio>
#include <cstdlib>

#include <glog/logging.h>
#include <gtest/gtest.h>
#include <iostream>

#include "Master.h"
#include "MasterClient.h"

namespace paradigm4 {
namespace pico {
namespace core {



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

    client.clear_master();
    client.finalize();
    master.exit();
    master.finalize();
}

TEST(MasterTest, RegisterNode) {
    Master master("127.0.0.1");
    master.initialize();
    std::string RANK_KEY="asdf";

    TcpMasterClient pserver1(master.endpoint());
    pserver1.initialize();
    CommInfo info;
    info.global_rank = pserver1.generate_id(RANK_KEY);
    info.endpoint = "127.0.0.1:1";
    pserver1.register_node(info);
    EXPECT_EQ(0, info.global_rank);

    TcpMasterClient pserver2(master.endpoint());
    pserver2.initialize();
    info.global_rank = pserver2.generate_id(RANK_KEY);
    info.endpoint = "127.0.0.1:2";
    pserver1.register_node(info);
    EXPECT_EQ(1, info.global_rank);

    TcpMasterClient client1(master.endpoint());
    client1.initialize();
    info.global_rank = client1.generate_id(RANK_KEY);
    info.endpoint = "127.0.0.1:101";
    client1.register_node(info);
    EXPECT_EQ(2, info.global_rank);

    TcpMasterClient client2(master.endpoint());
    client2.initialize();
    info.global_rank = client2.generate_id(RANK_KEY);
    info.endpoint = "127.0.0.1:102";
    client2.register_node(info);
    EXPECT_EQ(3, info.global_rank);

    TcpMasterClient client3(master.endpoint());
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
    /*
    Master master("127.0.0.1");
    master.initialize();
    TcpMasterClient client(master.endpoint());
    client.initialize();

    TcpMasterClient client1(master.endpoint());
    client1.initialize();
    client1.register_node({0, "127.0.0.1:1"});

    TcpMasterClient client2(master.endpoint());
    client2.initialize();
    client2.register_node({1, "127.0.0.1:2"});
    
    TcpMasterClient client3(master.endpoint());
    client3.initialize();
    client3.register_node({2, "127.0.0.1:3"});
    
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
    }


    client3.finalize();
    client2.finalize();
    client1.finalize();

    client.clear_master();
    client.finalize();
    master.exit();
    master.finalize();*/
}


} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
