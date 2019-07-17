#include <cstdio>
#include <cstdlib>

#include <atomic>

#include <unistd.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include "RpcService.h"
#include "macro.h"
#include <sys/prctl.h>

namespace paradigm4 {
namespace pico {
namespace core {
using namespace std::chrono_literals;

const int kMaxRetry = 100;

class EchoService {
public:
    EchoService(std::string master_ep) {
        _mc = std::make_unique<TcpMasterClient>(master_ep);
        while (!_mc->initialize());
        RpcConfig rpc_config;
        rpc_config.protocol = "tcp";
        rpc_config.bind_ip = "127.0.0.1";
        rpc_config.io_thread_num = 1;
        _rpc = std::make_unique<RpcService>();
        _rpc->initialize(_mc.get(), rpc_config);
        _server = _rpc->create_server("asdf");
    }

    ~EchoService() {
        _server.reset();
        _rpc->finalize();
        _mc->finalize();
    }


    void run() {
        std::vector<std::thread> ths(2);
        for (auto& th : ths) {
            th = std::thread(
                  [&]() {
                      auto dealer = _server->create_dealer();
                      RpcRequest req;
                      std::string tmp;
                      while (dealer->recv_request(req)) {
                          req >> tmp;
                          RpcResponse resp(req);
                          resp << tmp;
                          dealer->send_response(std::move(resp));
                      }
                  });
        }
        for (auto& th : ths) {
            th.join();
        }
    }

    void terminate() {
        _server->terminate();
    }

    std::unique_ptr<TcpMasterClient> _mc;
    std::unique_ptr<RpcService> _rpc;
    std::unique_ptr<RpcServer> _server;
};

class SimpleClient {
public:
    std::string _master_ep;
    SimpleClient(std::string master_ep) {
        _master_ep = master_ep;
    }

    void run(int n) {
        auto mc = std::make_unique<TcpMasterClient>(_master_ep);
        while (!mc->initialize());
        RpcConfig rpc_config;
        rpc_config.protocol = "tcp";
        rpc_config.bind_ip = "127.0.0.1";
        rpc_config.io_thread_num = 1;

        rpc_config.tcp.keepalive_intvl = -1;
        rpc_config.tcp.keepalive_probes = -1;
        rpc_config.tcp.keepalive_time = -1;
        rpc_config.tcp.connect_timeout = 5;
        TcpSocket::set_tcp_config(rpc_config.tcp);
        auto rpc = std::make_unique<RpcService>();
        rpc->initialize(mc.get(), rpc_config);
        SLOG(INFO) << "waiting one server";
        auto client = rpc->create_client("asdf", 1);
        auto func = [&]() {
            auto dealer = client->create_dealer();
            size_t timeout_cnt = 0, error_cnt = 0;
            size_t total = 0;
            for (int i = 0; i < n; ++i) {
                SLOG(INFO) << "timeout/err/total : " << timeout_cnt << " / "
                    << error_cnt << " / " << total;

                RpcRequest req;
                RpcResponse resp;
                std::string a = "asdf", b = "fdsas";
                req << a;
                dealer->send_request(std::move(req));
                ++total;
                if (dealer->recv_response(resp, 10)) {
                    if (resp.error_code() != RpcErrorCodeType::SUCC) {
                        SLOG(WARNING)
                              << "get error : " << (int)resp.error_code();
                        dealer = client->create_dealer();
                        ++error_cnt;
                    } else {
                        resp >> b;
                        EXPECT_TRUE(b == a);
                    }
                } else {
                    dealer = client->create_dealer();
                    SLOG(WARNING) << "timeout!";
                    ++timeout_cnt;
                }
            }
            SLOG(INFO) << "timeout/err/total : " << timeout_cnt << " / "
                << error_cnt << " / " << total;
        };
        std::thread t1 = std::thread(func);
        std::thread t2 = std::thread(func);
        t1.join();
        t2.join();
        rpc->finalize();
        mc->finalize();
    }
};


class ProcessKiller {
public:
    void run() {
        _stop.store(false);
        killer = std::thread([&]() {
            while (true) {
                std::this_thread::sleep_for((rand() % 100) * 1ms);
                pid_t pid;
                {
                    std::lock_guard<std::mutex> _(mu);
                    if (children.empty()) {
                        if (_stop.load()) {
                            break;
                        } else {
                            continue;
                        }
                    }
                    std::swap(children.back(),
                          children[rand() % children.size() / 3]);
                    pid = children.back();
                    children.pop_back();
                }
                SLOG(INFO) << "kill pid : " << pid;
                kill(pid, SIGTERM);
                int status;
                pid = waitpid(pid, &status, 0);
                PSCHECK(pid != -1);
            }

            for (auto pid : children) {
                kill(pid, SIGTERM);
                int status;
                pid = waitpid(pid, &status, 0);
                PSCHECK(pid != -1);
            }
            SLOG(INFO) << "killer over";
        });
    }

    void add(pid_t pid) {
        std::lock_guard<std::mutex> _(mu);
        children.push_back(pid);
    }

    void terminate() {
        _stop.store(true);
        killer.join();
    }

    std::vector<pid_t> children;
    std::mutex mu;
    std::atomic<bool> _stop;
    std::thread killer;
};


TEST(RpcService, multithread) {
    std::unique_ptr<Master> master;
    master = std::make_unique<Master>("127.0.0.1");
    master->initialize();
    auto master_ep = master->endpoint();
    {
        EchoService echo_server(master_ep);
        std::thread server = std::thread([&]() { echo_server.run(); });
        std::thread client = std::thread([&]() {
            SimpleClient client(master_ep);
            client.run(10000);
        });
        client.join();
        echo_server.terminate();
        server.join();
    }
    master->exit();
    master->finalize();
}



TEST(RpcService, killserver) {
    Logger::singleton().set_id("main");
    std::unique_ptr<Master> master;
    master = std::make_unique<Master>("127.0.0.1");
    master->initialize();
    auto master_ep = master->endpoint();
    ProcessKiller killer;
    killer.run();

    std::thread t1 = std::thread([&]() {
        std::this_thread::sleep_for((rand() % 100) * 1ms);
        SimpleClient c(master_ep);
        c.run(100000);
    });

    std::thread t2 = std::thread([&]() {
        std::this_thread::sleep_for((rand() % 100) * 1ms);
        SimpleClient c(master_ep);
        c.run(100000);
    });


    for (int i = 0; i < 100; ++i) {
        pid_t fpid = fork();
        if (fpid == 0) {
            // child
            Logger::singleton().set_id(std::to_string(i));
            EchoService server(master_ep);
            server.run();
            exit(0);
        } else {
            killer.add(fpid);
        }
        std::this_thread::sleep_for((rand() % 100) * 1ms);
    }
    t1.join();
    t2.join();
    killer.terminate();
    master->exit();
    master->finalize();
}

TEST(RpcService, killclient) {
    std::unique_ptr<Master> master;
    master = std::make_unique<Master>("127.0.0.1");
    master->initialize();
    auto master_ep = master->endpoint();
    {
        ProcessKiller killer;
        killer.run();

        EchoService s1(master_ep);
        EchoService s2(master_ep);
        std::thread t1 = std::thread([&]() {
            std::this_thread::sleep_for((rand() % 100) * 1ms);
            s1.run();
        });

        std::thread t2 = std::thread([&]() {
            std::this_thread::sleep_for((rand() % 100) * 1ms);
            s2.run();
        });

        Logger::singleton().set_id("main");
        for (int i = 0; i < 100; ++i) {
            pid_t fpid = fork();
            if (fpid == 0) {
                // child
                Logger::singleton().set_id(std::to_string(i));
                SimpleClient c(master_ep);
                c.run(100000);
                exit(0);
            } else {
                killer.add(fpid);
            }
            std::this_thread::sleep_for((rand() % 100) * 1ms);
        }
        killer.terminate();
        s1.terminate();
        s2.terminate();

        t1.join();
        t2.join();
    }
    SLOG(INFO) << "hererererere";
    master->exit();
    SLOG(INFO) << "hererererere";
    master->finalize();
    SLOG(INFO) << "hererererere";
}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
#ifdef PR_SET_PTRACER
    prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
#endif
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
