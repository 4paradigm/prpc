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
    }

    ~EchoService() {
        _rpc->finalize();
        _mc->finalize();
    }


    void run() {
        auto server = _rpc->create_server("asdf");
        std::vector<std::thread> ths(2);
        for (auto& th : ths) {
            th = std::thread(
                  [&]() {
                      auto dealer = server->create_dealer();
                      RpcRequest req;
                      std::string tmp;
                      std::string id = Logger::singleton().get_id();
                      int cnt = 0;
                      int max_server = rand();
                      while (dealer->recv_request(req)) {
                          req >> tmp;
                          SLOG(INFO) << "Server " << server->id() << " : "
                                     << ++cnt << " ; max : " << max_server;
                          if (cnt > max_server) {
                              SLOG(FATAL) << "Server crash!!!!!";
                          }
                          RpcResponse resp(req);
                          resp << id;
                          dealer->send_response(std::move(resp));
                      }
                  });
        }
        for (auto& th : ths) {
            th.join();
        }
    }

    std::unique_ptr<RpcService> _rpc;
    std::unique_ptr<TcpMasterClient> _mc;
};

class SimpleClient {
public:
    std::string _master_ep;
    SimpleClient(std::string master_ep) {
        _master_ep = master_ep;
    }

    void run() {
        auto mc = std::make_unique<TcpMasterClient>(_master_ep);
        while (!mc->initialize());
        RpcConfig rpc_config;
        rpc_config.protocol = "tcp";
        rpc_config.bind_ip = "127.0.0.1";
        rpc_config.io_thread_num = 1;
        rpc_config.tcp.connect_timeout = 5;
        auto rpc = std::make_unique<RpcService>();
        rpc->initialize(mc.get(), rpc_config);
        SLOG(INFO) << "waiting one server";
        auto client = rpc->create_client("asdf", 1);
        size_t timeout_cnt = 0, error_cnt = 0;
        size_t total = 0;
        auto func = [&]() {
            auto dealer = client->create_dealer();
            while (true) {
                RpcRequest req;
                RpcResponse resp;
                std::string a = "asdf", b = "fdsas";
                req << a;
                SLOG(INFO) << "timeout/err/total : " << timeout_cnt << " / "
                           << error_cnt << " / " << total;
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
                        SLOG(INFO) << b;
                    }
                } else {
                    dealer = client->create_dealer();
                    SLOG(WARNING) << "timeout!";
                    ++timeout_cnt;
                }
            }
        };
        std::thread t1 = std::thread(func);
        std::thread t2 = std::thread(func);
        t1.join();
        t2.join();
        SLOG(INFO) << "timeout/err/total : " << timeout_cnt << " / "
                   << error_cnt << " / " << total;
        rpc->finalize();
        mc->finalize();
    }
};

TEST(RpcService, multiprocess) {
    std::unique_ptr<Master> master;
    master = std::make_unique<Master>("127.0.0.1");
    master->initialize();
    auto master_ep = master->endpoint();

    std::mutex mu;
    std::vector<pid_t> children;
    std::atomic<bool> stop(false);

    std::thread killer = std::thread([&]() {
        while (!stop.load()) {
            std::this_thread::sleep_for(100ms);
            pid_t pid;
            {
                std::lock_guard<std::mutex> _(mu);
                if (children.empty()) {
                    continue;
                }
                std::swap(children.back(), children[rand() % children.size() / 3]);
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
    });

    SLOG(INFO) << "my pid is " << getpid();
    Logger::singleton().set_id("main");
    for (int i = 0; i < 100; ++i) {
        pid_t fpid = fork();
        if (fpid == 0) {
            // child
            Logger::singleton().set_id(std::to_string(i));
            if (rand() % 2) {
                EchoService server(master_ep);
                server.run();
            } else {
                SimpleClient client(master_ep);
                client.run();
            }
            exit(0);
        } else {
            std::lock_guard<std::mutex> _(mu);
            children.push_back(fpid);
        }
        std::this_thread::sleep_for(30ms);
    }
    while (true) {
        {
            lock_guard<std::mutex> _(mu);
            if (children.empty()) {
                break;
            }
        }
        sleep(1);
    }
    stop.store(true);
    killer.join();
    master->exit();
    master->finalize();
}



} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
