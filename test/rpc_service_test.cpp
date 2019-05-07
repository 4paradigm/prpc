#include <cstdio>
#include <cstdlib>

#include <atomic>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "Barrier.h"
#include "RpcService.h"
#include "initialize.h"
#include "macro.h"
#include "pico_test_common.h"
#include "pico_unittest_operator.h"

namespace paradigm4 {
namespace pico {

const int kMaxRetry = 100;

TEST(RpcService, check_ok) {
    std::thread server_thread;
    std::thread client_thread;
    pico_barrier(PICO_FILE_LINENUM);
    pico_rpc_register_service("test");
    if (pico_comm_rank() == 0) {
        auto server_run = []() {
            Dealer dealer = pico_rpc_service().create_dealer("test", pico_rpc_service().role());
            dealer.initialize_as_server();
            RpcRequest request;
            while (dealer.recv_request(request)) {
                std::string msg_str;
                request >> msg_str;
                std::string check_str = format_string("%d:request", 123);
                EXPECT_STREQ(msg_str.c_str(), check_str.c_str());

                RpcResponse response(request);
                std::string rep_str = format_string("%d:response", pico_comm_rank());
                response << rep_str;
                dealer.send_response(std::move(response));
            }
            dealer.finalize();
        };
        server_thread = std::thread(server_run);
    }

    pico_barrier(PICO_FILE_LINENUM);

    auto client_run = []() {
        int32_t dest_id = 0;
        RpcRequest request(dest_id);
        std::string msg_str = format_string("%d:request", 123);
        request << msg_str;
        RpcResponse response = pico_rpc_service().create_client_dealer("test", pico_rpc_service().role())
                                     .sync_rpc_call(std::move(request));
        std::string rep_str;
        response >> rep_str;
        std::string check_str = format_string("%d:response", response.head().src_rank);
    };

    client_thread = std::thread(client_run);

    client_thread.join();

    pico_barrier(PICO_FILE_LINENUM);

    pico_rpc_terminate_service("test");
    pico_rpc_deregister_service("test");

    if (pico_comm_rank() == 0) {
        server_thread.join();
    }
}


TEST(RpcService, terminate) {
    for (int i = 0; i < kMaxRetry; ++i) {
        auto server_run = [](const std::string& rpc_name) {
            Dealer dealer = pico_rpc_service().create_dealer(rpc_name, pico_rpc_service().role());
            dealer.initialize_as_server();
            RpcRequest request;
            while (dealer.recv_request(request)) {
                int t;
                request >> t;
                RpcResponse response(request);
                response << t;
                dealer.send_response(std::move(response));
            }
            dealer.finalize();
        };

        std::string rpc_name = "test_rpc";
        pico_barrier(PICO_FILE_LINENUM);
        pico_rpc_register_service(rpc_name);
        std::thread server_thread;

        server_thread = std::thread(server_run, rpc_name);
        pico_barrier(PICO_FILE_LINENUM);
        pico_rpc_terminate_service(rpc_name);
        server_thread.join();
        pico_rpc_deregister_service(rpc_name);
    }
    pico_barrier(PICO_FILE_LINENUM);

}


TEST(RpcService, ping) {
    auto server_run = [](const std::string& rpc_name) {
        Dealer dealer = pico_rpc_service().create_dealer(rpc_name, pico_rpc_service().role());
        dealer.initialize_as_server();
        RpcRequest request;
        while (dealer.recv_request(request)) {
            std::vector<int> t1, t2;
            request >> t1;
            request.lazy() >> t2;
            RpcResponse response(request);
            response << t1;
            response.lazy() << std::move(t2);
            dealer.send_response(std::move(response));
        }
        dealer.finalize();
    };

    auto client_run = [](const std::string& rpc_name) {
        Dealer dealer = pico_rpc_service().create_dealer(rpc_name, pico_rpc_service().role());
        dealer.initialize_as_client();

        for (int k = 0; k < kMaxRetry; ++k) {
            int32_t dest_id = (pico_comm_rank() + k) % pico_comm_size();
            std::vector<int> t(1024*1024+k);
            RpcRequest request(dest_id);
            request << t;
            request.lazy() << std::move(t);
            RpcResponse response = dealer.sync_rpc_call(std::move(request));
            std::vector<int> r1, r2;
            response >> r1;
            response.lazy() >> r2;
            EXPECT_EQ(r2.size(), r1.size());
        }
        dealer.finalize();
    };

    std::string rpc_name = "test_rpc";
    pico_barrier(PICO_FILE_LINENUM);
    pico_rpc_register_service(rpc_name);
    std::thread server_thread;
    std::thread client_thread;

    server_thread = std::thread(server_run, rpc_name);
    client_thread = std::thread(client_run, rpc_name);
    client_thread.join();

    pico_barrier(PICO_FILE_LINENUM);
    pico_rpc_terminate_service(rpc_name);
    pico_rpc_deregister_service(rpc_name);
    server_thread.join();
}

TEST(RpcService, CreateDealer) {
    auto server_run = [](const std::string& rpc_name) {
        Dealer dealer = pico_rpc_service().create_dealer(rpc_name, pico_rpc_service().role());
        dealer.initialize_as_server();
        RpcRequest request;
        while (dealer.recv_request(request)) {
            int t;
            request >> t;
            RpcResponse response(request);
            response << t;
            dealer.send_response(std::move(response));
        }
        dealer.finalize();
    };

    auto client_run = [](const std::string& rpc_name, int k) {
        int32_t dest_id = (pico_comm_rank() + k) % pico_comm_size();
        RpcRequest request(dest_id);
        request << k;
        RpcResponse response = pico_rpc_service().create_client_dealer(rpc_name, pico_rpc_service().role())
                                     .sync_rpc_call(std::move(request));
        int r;
        response >> r;
        EXPECT_EQ(k, r);
    };

    std::string rpc_name = "test_rpc";
    pico_barrier(PICO_FILE_LINENUM);
    pico_rpc_register_service(rpc_name);
    std::thread server_thread;
    std::thread client_thread;

    server_thread = std::thread(server_run, rpc_name);
    for (int i = 0; i < kMaxRetry; ++i) {
        // SLOG(INFO) << i;
        client_thread = std::thread(client_run, rpc_name, i);
        client_thread.join();
    }

    pico_barrier(PICO_FILE_LINENUM);
    pico_rpc_terminate_service(rpc_name);
    pico_rpc_deregister_service(rpc_name);
    server_thread.join();
}

TEST(RpcService, RegisterServiceNoMessage) {
    for (int i = 0; i < kMaxRetry; ++i) {
        std::string rpc_name = format_string("test_rpc_%d", i);
        pico_barrier(PICO_FILE_LINENUM);
        pico_rpc_register_service(rpc_name);
        pico_barrier(PICO_FILE_LINENUM);
        pico_rpc_deregister_service(rpc_name);
    }
}

TEST(RpcService, RegisterServiceNoClient) {
    auto server_run = [](const std::string& rpc_name, std::atomic<bool>& flag) {
        Dealer dealer = pico_rpc_service().create_dealer(rpc_name, pico_rpc_service().role());
        dealer.initialize_as_server();
        flag = false;
        RpcRequest request;
        while (dealer.recv_request(request)) {
            // do nothing
        }
        dealer.finalize();
    };
    for (int i = 0; i < kMaxRetry; ++i) {
        std::string rpc_name = format_string("test_rpc_%d", i);
        pico_barrier(PICO_FILE_LINENUM);
        pico_rpc_register_service(rpc_name);
        std::atomic<bool> flag(true);
        auto server_thread = std::thread(server_run, rpc_name, std::ref(flag));
        while (flag) {
        }
        pico_barrier(PICO_FILE_LINENUM);
        pico_rpc_terminate_service(rpc_name);
        pico_rpc_deregister_service(rpc_name);
        server_thread.join();
    }
}

TEST(RpcService, RegisterService) {
    auto server_run = [](const std::string& rpc_name) {
        Dealer dealer = pico_rpc_service().create_dealer(rpc_name, pico_rpc_service().role());
        dealer.initialize_as_server();
        RpcRequest request;
        while (dealer.recv_request(request)) {
            int t;
            request >> t;
            RpcResponse response(request);
            response << t;
            dealer.send_response(std::move(response));
        }
        dealer.finalize();
    };

    auto client_run = [](const std::string& rpc_name, int k) {
        int32_t dest_id = (pico_comm_rank() + k) % pico_comm_size();
        RpcRequest request(dest_id);
        request << k;
        RpcResponse response = pico_rpc_service().create_client_dealer(rpc_name, pico_rpc_service().role())
                                     .sync_rpc_call(std::move(request));
        int r;
        response >> r;
        EXPECT_EQ(k, r);
    };

    for (int i = 0; i < kMaxRetry; ++i) {
        std::string rpc_name = format_string("test_rpc_%d", i);
        pico_barrier(PICO_FILE_LINENUM);
        pico_rpc_register_service(rpc_name);
        auto server_thread = std::thread(server_run, rpc_name);
        auto client_thread = std::thread(client_run, rpc_name, i);
        client_thread.join();
        pico_barrier(PICO_FILE_LINENUM);
        pico_rpc_terminate_service(rpc_name);
        pico_rpc_deregister_service(rpc_name);
        server_thread.join();
        pico_barrier(PICO_FILE_LINENUM);
    }
}

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    paradigm4::pico::test::PicoUnitTestCommon::singleton().initialize(&argc, argv);
    if (paradigm4::pico::test::PicoUnitTestOperator::singleton().is_show_operator()) {
        // local_wrapper, repeat_num=10
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(
              paradigm4::pico::test::LocalWrapperOperator(10, 0));
        // mpirun, repeat_num=10, np={3, 5, 8}
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(
              paradigm4::pico::test::MpiRunOperator(10, 0, {3, 5, 8}));
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
