#include <cstdio>
#include <cstdlib>

#include <glog/logging.h>
#include <gtest/gtest.h>
#include <iostream>

#include "Master.h"
#include "MasterClient.h"
#include "RpcService.h"
#include "Barrier.h"
#include "initialize.h"

#include "pico_test_common.h"
#include "pico_unittest_operator.h"

namespace paradigm4 {
namespace pico {

TEST(RpcTest, ok) {
    Master master("127.0.0.1");
    master.initialize();
    SLOG(INFO) << "Master initialized.";
    TcpMasterClient master_client(master.endpoint());
    master_client.initialize();
    SLOG(INFO) << "Client initialized.";

    RpcService rpc;
    RpcConfig rpc_config;
    rpc_config.wrapper_ip = "127.0.0.1";
    rpc.initialize(&master_client, rpc_config);

    std::unique_ptr<RpcServer> server = rpc.create_server("asdf");
    std::unique_ptr<RpcClient> client = rpc.create_client("asdf", 1);
    std::shared_ptr<Dealer> s_dealer = server->create_dealer();
    //server.remove(s_dealer);
    auto c_dealer = client->create_dealer();

    std::thread client_th = std::thread([&]() {
        RpcRequest req;
        RpcResponse resp;
        std::string s = "asdfasdfasdfasf", e;
        req << s;
        c_dealer->send_request(std::move(req));
        bool ret = c_dealer->recv_response(resp);
        SCHECK(ret);
        resp >> e;
        SCHECK(s == e);
    });

    std::thread server_th = std::thread([&]() {
        RpcRequest req;
        std::string s;
        s_dealer->recv_request(req);
        RpcResponse resp(req);
        req >> s;
        resp << s;
        s_dealer->send_response(std::move(resp));
    });

    client_th.join();
    server_th.join();
    c_dealer.reset();
    s_dealer.reset();
    server.reset();
    client.reset();
    rpc.finalize();
    master_client.clear_master();
    master_client.finalize();
    master.exit();
    master.finalize();
}

TEST(RpcTest, ok2) {
    Master master("127.0.0.1");
    master.initialize();
    SLOG(INFO) << "Master initialized.";
    TcpMasterClient master_client(master.endpoint());
    master_client.initialize();
    SLOG(INFO) << "Client initialized.";

    RpcService rpc;
    RpcConfig rpc_config;
    rpc_config.wrapper_ip = "127.0.0.1";
    rpc.initialize(&master_client, rpc_config);

    auto s1 = rpc.create_server("asdf");
    auto s2 = rpc.create_server("asdf");
    auto client = rpc.create_client("asdf", 2);
    {
        auto s1_dealer = s1->create_dealer();
        auto s2_dealer = s2->create_dealer();
        auto c_dealer = client->create_dealer();

        std::thread client_th = std::thread([&]() {
            RpcRequest req;
            RpcResponse resp;
            std::string s = "asdfasdfasdfasf", e;
            req.head().sid = 0;
            req << s;
            c_dealer->send_request(std::move(req));
            bool ret = c_dealer->recv_response(resp);
            SCHECK(ret);
            resp >> e;
            SCHECK(s == e);

            req.head().sid = 1;
            req << s;
            c_dealer->send_request(std::move(req));
            ret = c_dealer->recv_response(resp);
            SCHECK(ret);
            resp >> e;
            SCHECK(s == e);
        });

        std::thread s1_th = std::thread([&]() {
            RpcRequest req;
            std::string s;
            s1_dealer->recv_request(req);
            RpcResponse resp(req);
            req >> s;
            resp << s;
            s1_dealer->send_response(std::move(resp));
        });

        std::thread s2_th = std::thread([&]() {
            RpcRequest req;
            std::string s;
            s2_dealer->recv_request(req);
            RpcResponse resp(req);
            req >> s;
            resp << s;
            s2_dealer->send_response(std::move(resp));
        });

        client_th.join();
        s1_th.join();
        s2_th.join();
    }

    s1.reset();
    s2.reset();
    client.reset();
    rpc.finalize();
    master_client.clear_master();
    master_client.finalize();
    master.exit();
    master.finalize();
}

TEST(RpcTest, p2p) {
    Master master("127.0.0.1");
    master.initialize();
    SLOG(INFO) << "Master initialized.";
    TcpMasterClient master_client(master.endpoint());
    master_client.initialize();
    SLOG(INFO) << "Client initialized.";

    RpcService rpc;
    RpcConfig rpc_config;
    rpc_config.wrapper_ip = "127.0.0.1";
    rpc.initialize(&master_client, rpc_config);
    rpc.update_ctx();

    auto a = rpc.create_dealer("asdf");
    std::string s = "asdfasdfasdfa", e;
    {
        RpcRequest req;
        req.head().dest_rank = rpc.global_rank();
        req << s;
        a->send_request(std::move(req));
    }

    {
        RpcRequest req;
        std::string e;
        a->recv_request(req);
        RpcResponse resp(req);
        req >> e;
        SLOG(INFO) << e;
        resp << e;
        a->send_response(std::move(resp));
    }

    {
        RpcResponse resp;
        bool ret = a->recv_response(resp);
        SCHECK(ret);
        resp >> e;
        SCHECK(s == e) << s << " " << e;
    }

    rpc.finalize();
    master_client.clear_master();
    master_client.finalize();
    master.exit();
    master.finalize();
}

TEST(RpcTest, haha) {
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

    paradigm4::pico::pico_initialize(argc, argv);
    int ret = RUN_ALL_TESTS();
    paradigm4::pico::pico_finalize();
    return ret;
}
