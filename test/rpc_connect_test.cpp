#include <cstdio>
#include <cstdlib>

#include <atomic>

#include <unistd.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include "RpcService.h"
#include "macro.h"

namespace paradigm4 {
namespace pico {
namespace core {

class FakeMaster {
public:
    FakeMaster() {
        _master = std::make_unique<Master>("127.0.0.1");
        _master->initialize();
    }
    ~FakeMaster() {
        _master->exit();
        _master->finalize();
    }
    Master* master() {
        return _master.get();
    }
    std::unique_ptr<Master> _master;
};

class FakeRpc {
public:
    FakeRpc(Master* master) {
        auto master_ep = master->endpoint();
        _mc = std::make_unique<TcpMasterClient>(master_ep);
        _mc->initialize();
        RpcConfig rpc_config;
        rpc_config.protocol = "tcp";
        rpc_config.bind_ip = "127.0.0.1";
        rpc_config.io_thread_num = 1;
        _rpc = std::make_unique<RpcService>();
        _rpc->initialize(_mc.get(), rpc_config);
    }

    ~FakeRpc() {
        _rpc->finalize();
        _mc->finalize();
    }

    RpcService* rpc() {
        return _rpc.get();
    }

    std::unique_ptr<RpcService> _rpc;
    std::unique_ptr<TcpMasterClient> _mc;
};


TEST(RpcService, Connect) {
    int nodes = 20;
    FakeMaster master;
    auto server_run = [=](RpcServer* server) {
        auto dealer = server->create_dealer();
        RpcRequest req;
        while (dealer->recv_request(req)) {
            RpcResponse resp(req);
            dealer->send_response(std::move(resp));
        }
    };

    auto client_run = [=](RpcService* rpc) {
        auto client = rpc->create_client("asdfasdf", nodes);
        auto dealer = client->create_dealer();
        for (int i = 0; i < nodes * 3; ++i) {
            RpcRequest req;
            req.head().sid = rand() % nodes;
            dealer->send_request(std::move(req));
            RpcResponse resp;
            dealer->recv_response(resp);
        }
    };
    std::unique_ptr<FakeRpc> rpc[nodes];
    std::unique_ptr<RpcServer> server[nodes];
    std::thread server_thread[nodes];
    std::thread client_thread[nodes];
    for (int i = 0; i < nodes; ++i) {
        rpc[i] = std::make_unique<FakeRpc>(master.master());
        server[i] = rpc[i]->rpc()->create_server("asdfasdf");
    }
    for (int i = 0; i < nodes; ++i) {
        server_thread[i] = std::thread(server_run, server[i].get());
        client_thread[i] = std::thread(client_run, rpc[i]->rpc());
    }
    for (int i = 0; i < nodes; ++i) {
        client_thread[i].join();
    }
    for (int i = 0; i < nodes; ++i) {
        server[i]->terminate();
        server_thread[i].join();
        server[i].reset();
        rpc[i].reset();
    }
}

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}



}
}
}