#include <cstdio>
#include <cstdlib>

#include <atomic>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "RpcService.h"
#include "macro.h"

namespace paradigm4 {
namespace pico {
namespace core {

const int kMaxRetry = 100;

class FakeRpc {
public:
    FakeRpc() {
        _master = std::make_unique<Master>("127.0.0.1");
        _master->initialize();
        auto master_ep = _master->endpoint();
        _mc1 = std::make_unique<TcpMasterClient>(master_ep);
        _mc2 = std::make_unique<TcpMasterClient>(master_ep);
        _mc1->initialize();
        _mc2->initialize();
        RpcConfig rpc_config;
        rpc_config.protocol = "tcp";
        rpc_config.bind_ip = "127.0.0.1";
        rpc_config.io_thread_num = 1;
        _rpc1 = std::make_unique<RpcService>();
        _rpc2 = std::make_unique<RpcService>();
        _rpc1->initialize(_mc1.get(), rpc_config);
        _rpc2->initialize(_mc2.get(), rpc_config);
    }

    ~FakeRpc() {
        _rpc1->finalize();
        _rpc2->finalize();
        _mc1->finalize();
        _mc2->finalize();
        _master->exit();
        _master->finalize();
    }

    RpcService* rpc1() {
        return _rpc1.get();
    }

    RpcService* rpc2() {
        return _rpc2.get();
    }

    std::unique_ptr<Master> _master;
    std::unique_ptr<RpcService> _rpc1, _rpc2;
    std::unique_ptr<TcpMasterClient> _mc1, _mc2;
};

TEST(RpcService, check_ok) {
    std::thread server_thread;
    std::thread client_thread;
    auto server_run = [](RpcService* rpc) {
        auto server = rpc->create_server("asdf");
        auto dealer = server->create_dealer();
        RpcRequest request;
        for (int i = 0; i < kMaxRetry; ++i) {
            if (dealer->recv_request(request)) {
                std::string msg_str;
                request >> msg_str;
                std::string check_str = format_string("%d:request", 123);
                EXPECT_STREQ(msg_str.c_str(), check_str.c_str());
                RpcResponse response(request);
                std::string rep_str = format_string("%d:response", 123);
                response << rep_str;
                dealer->send_response(std::move(response));
            }
        }
        dealer.reset();
    };

    auto client_run = [](RpcService* rpc) {
        for (int i = 0; i < kMaxRetry; ++i) {
            RpcRequest request;
            std::string msg_str = format_string("%d:request", 123);
            request << msg_str;
            RpcResponse response
                = rpc->create_client("asdf", 1)->create_dealer()->sync_rpc_call(
                        std::move(request));
            std::string rep_str;
            response >> rep_str;
            std::string check_str = format_string("%d:response", 123);
        }
    };
    FakeRpc rpc;
    server_thread = std::thread(server_run, rpc.rpc1());
    client_thread = std::thread(client_run, rpc.rpc2());
    client_thread.join();
    server_thread.join();
}

TEST(RpcService, RegisterService) {
    auto server_run = [](RpcService* rpc) {
        for (int i = 0; i < kMaxRetry; ++i) {
            auto server = rpc->create_server("asdf");
            auto dealer = server->create_dealer();
            RpcRequest request;
            if (dealer->recv_request(request)) {
                std::string msg_str;
                request >> msg_str;
                std::string check_str = format_string("%d:request", 123);
                EXPECT_STREQ(msg_str.c_str(), check_str.c_str());
                RpcResponse response(request);
                std::string rep_str = format_string("%d:response", 123);
                response << rep_str;
                dealer->send_response(std::move(response));
            }
            dealer.reset();
        }
    };

    auto client_run = [](RpcService* rpc) {
        for (int i = 0; i < kMaxRetry; ++i) {
            while (true) {
                RpcRequest request;
                std::string msg_str = format_string("%d:request", 123);
                request << msg_str;

                auto client = rpc->create_client("asdf", 0);
                auto dealer = client->create_dealer();
                dealer->send_request(std::move(request));

                RpcResponse response;
                if (!dealer->recv_response(response, 10000)) {
                    continue;
                }
                if (response.error_code() != 0) {
                    continue;
                }
                std::string rep_str;
                response >> rep_str;
                std::string check_str = format_string("%d:response", 123);
                break;
            }
        }
    };
    FakeRpc rpc;
    auto server_thread = std::thread(server_run, rpc.rpc1());
    auto client_thread = std::thread(client_run, rpc.rpc2());
    client_thread.join();
    server_thread.join();
}

TEST(RpcService, SmallMessage) {
    auto server_run = [](RpcService* rpc) {
        auto server = rpc->create_server("asdf");
        auto dealer = server->create_dealer();
        std::string check_str;
        check_str.resize(128);
        for (int i = 0; i < kMaxRetry; ++i) {
            RpcRequest request;
            if (dealer->recv_request(request)) {
                std::string msg_str;
                request >> msg_str;
                EXPECT_STREQ(msg_str.c_str(), check_str.c_str());
                RpcResponse response(request);
                response << check_str;
                dealer->send_response(std::move(response));
            }
        }
    };

    auto client_run = [](RpcService* rpc) {
        std::string check_str;
        check_str.resize(128);
        for (int i = 0; i < kMaxRetry; ++i) {
            RpcRequest request;
            request << check_str;

            auto client = rpc->create_client("asdf", 1);
            auto dealer = client->create_dealer();
            dealer->send_request(std::move(request));

            RpcResponse response;
            EXPECT_TRUE(dealer->recv_response(response));
            std::string rep_str;
            response >> rep_str;
            EXPECT_STREQ(rep_str.c_str(), check_str.c_str());
        }
    };
    FakeRpc rpc;
    auto server_thread = std::thread(server_run, rpc.rpc1());
    auto client_thread = std::thread(client_run, rpc.rpc2());
    client_thread.join();
    server_thread.join();
}

TEST(RpcService, BigMessage) {
    auto server_run = [](RpcService* rpc) {
        auto server = rpc->create_server("asdf");
        auto dealer = server->create_dealer();
        std::string check_str;
        check_str.resize(1024 * 1024 * 5);
        for (int i = 0; i < kMaxRetry; ++i) {
            RpcRequest request;
            if (dealer->recv_request(request)) {
                std::string msg_str;
                request >> msg_str;
                EXPECT_STREQ(msg_str.c_str(), check_str.c_str());
                RpcResponse response(request);
                response << check_str;
                dealer->send_response(std::move(response));
            }
        }
    };

    auto client_run = [](RpcService* rpc) {
        std::string check_str;
        check_str.resize(1024 * 1024 * 5);
        for (int i = 0; i < kMaxRetry; ++i) {
            RpcRequest request;
            request << check_str;

            auto client = rpc->create_client("asdf", 1);
            auto dealer = client->create_dealer();
            dealer->send_request(std::move(request));

            RpcResponse response;
            EXPECT_TRUE(dealer->recv_response(response));
            std::string rep_str;
            response >> rep_str;
            EXPECT_STREQ(rep_str.c_str(), check_str.c_str());
        }
    };
    FakeRpc rpc;
    auto server_thread = std::thread(server_run, rpc.rpc1());
    auto client_thread = std::thread(client_run, rpc.rpc2());
    client_thread.join();
    server_thread.join();
}

TEST(RpcService, RandMessage) {
    auto server_run = [](RpcService* rpc) {
        auto server = rpc->create_server("asdf");
        auto dealer = server->create_dealer();
        for (int i = 0; i < kMaxRetry; ++i) {
            RpcRequest request;
            if (dealer->recv_request(request)) {
                std::string msg_str;
                request >> msg_str;
                RpcResponse response(request);
                response << msg_str;
                dealer->send_response(std::move(response));
            }
        }
    };

    auto client_run = [](RpcService* rpc) {
        std::string check_str;
        for (int i = 0; i < kMaxRetry; ++i) {
            size_t sz = rand() * rand();
            sz %= 1024 * 1024 * 5;
            SLOG(INFO) << "send size : " << sz;
            check_str.resize(sz);
            RpcRequest request;
            request << check_str;

            auto client = rpc->create_client("asdf", 1);
            auto dealer = client->create_dealer();
            dealer->send_request(std::move(request));

            RpcResponse response;
            EXPECT_TRUE(dealer->recv_response(response));
            std::string rep_str;
            response >> rep_str;
            EXPECT_STREQ(rep_str.c_str(), check_str.c_str());
        }
    };
    FakeRpc rpc;
    auto server_thread = std::thread(server_run, rpc.rpc1());
    auto client_thread = std::thread(client_run, rpc.rpc2());
    client_thread.join();
    server_thread.join();
}

TEST(RpcService, LazyArchive) {
    auto server_run = [](RpcService* rpc) {
        auto server = rpc->create_server("asdf");
        auto dealer = server->create_dealer();
        for (int i = 0; i < kMaxRetry; ++i) {
            RpcRequest request;
            if (dealer->recv_request(request)) {
                std::string msg1, msg2;
                request.lazy() >> msg1;
                request >> msg2;
                EXPECT_EQ(msg1, msg2);
                RpcResponse response(request);
                response << msg1;
                response.lazy() << std::move(msg2);
                dealer->send_response(std::move(response));
            }
        }
    };

    auto client_run = [](RpcService* rpc) {
        std::string check_str;
        for (int i = 0; i < kMaxRetry; ++i) {
            size_t sz = rand() * rand();
            sz %= 5 * 1024 * 1024;
            check_str.resize(sz);
            SLOG(INFO) << "send size : " << sz;
            RpcRequest request;
            request << check_str;
            request.lazy() << std::move(check_str);

            auto client = rpc->create_client("asdf", 1);
            auto dealer = client->create_dealer();
            dealer->send_request(std::move(request));

            RpcResponse response;
            EXPECT_TRUE(dealer->recv_response(response));
            std::string aa, bb;
            response >> aa;
            response.lazy() >> bb;
            EXPECT_EQ(aa.size(), sz);
            EXPECT_EQ(bb.size(), sz);
            EXPECT_EQ(bb, aa);
        }
    };
    FakeRpc rpc;
    auto server_thread = std::thread(server_run, rpc.rpc1());
    auto client_thread = std::thread(client_run, rpc.rpc2());
    client_thread.join();
    server_thread.join();
}


TEST(RpcService, MultiThread) {
    int times = 10;
    int server_thread_num = 10;
    int client_thread_num = 20;
    auto server_run = [=](RpcService* rpc) {
        auto server = rpc->create_server("asdfasdf");
        auto dealer = server->create_dealer();
        int num = kMaxRetry * times * client_thread_num / server_thread_num;
        for (int i = 0; i < num; ++i) {
            RpcRequest request;
            if (dealer->recv_request(request)) {
                BinaryArchive ar1;
                std::string msg1, msg2;
                request.lazy() >> ar1;
                ar1 >> msg1;
                request >> msg2;
                EXPECT_EQ(msg1, msg2);
                RpcResponse response(request);
                BinaryArchive ar2(true);
                ar2 << msg2;
                response << msg1;
                response.lazy() << std::move(ar2);
                dealer->send_response(std::move(response));
            }
        }
    };

    auto client_run = [=](RpcService* rpc, int size) {
        std::string check_str;
        auto client = rpc->create_client("asdfasdf", server_thread_num);
        for (int i = 0; i < kMaxRetry * times; ++i) {
            size_t sz = rand() * rand();
            sz %= size;
            check_str.resize(sz);
            for (size_t i = 0; i < sz; ++i) {
                check_str[i] = 'A' + i % 26;
            }
            RpcRequest request;
            request.head().sid = i % server_thread_num;
            BinaryArchive ar1(true);
            ar1 << check_str;
            request << check_str;
            request.lazy() << std::move(ar1);

            auto dealer = client->create_dealer();
            dealer->send_request(std::move(request));

            RpcResponse response;
            EXPECT_TRUE(dealer->recv_response(response));
            BinaryArchive ar2;
            std::string aa, bb;
            response.lazy() >> ar2;
            response >> aa;
            ar2 >> bb;
            EXPECT_EQ(aa.size(), sz);
            EXPECT_EQ(bb.size(), sz);
            EXPECT_EQ(bb, aa);
        }
    };
    FakeRpc rpc;

    std::thread server_thread[server_thread_num];
    std::thread client_thread[client_thread_num];
    for (int i = 0; i < server_thread_num; ++i) {
        server_thread[i] = std::thread(server_run, rpc.rpc1());
    }
    for (int i = 0; i < client_thread_num; ++i) {
        client_thread[i] = std::thread(client_run, rpc.rpc2(), i % 3 ? 50 : 5 << 20);
    }
    for (int i = 0; i < server_thread_num; ++i) {
        server_thread[i].join();
    }
    for (int i = 0; i < client_thread_num; ++i) {
        client_thread[i].join();
    }
}




} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
