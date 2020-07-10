#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>
#include <sys/prctl.h>

#include "macro.h"
#include "Aggregator.h"
#include "SumAggregator.h"
#include "AvgAggregator.h"
#include "ArithmeticMaxAggregator.h"
#include "ArithmeticMinAggregator.h"
#include "TimerAggregator.h"
#include "Accumulator.h"
#include "MultiProcess.h"

namespace paradigm4 {
namespace pico {
namespace core {

class AccumulatorMultiProcess {
public:
    AccumulatorMultiProcess(size_t num): _num(num) {
        _master = std::make_unique<Master>("127.0.0.1");
        _master->initialize();
        p = std::make_unique<MultiProcess>(num);

        _mc = std::make_unique<TcpMasterClient>(_master->endpoint());
        _mc->initialize();
        RpcConfig rpc_config;
        rpc_config.protocol = "tcp";
        rpc_config.bind_ip = "127.0.0.1";
        rpc_config.io_thread_num = 1;
        _rpc = std::make_unique<RpcService>();
        _rpc->initialize(_mc.get(), rpc_config);

        if (_rpc->global_rank() == 0) {
            AccumulatorServer::singleton().initialize(_rpc.get());
        }
        AccumulatorClient::singleton().initialize(_rpc.get());
    }

    ~AccumulatorMultiProcess() {
        if (comm_rank() == 0) {
            AccumulatorClient::singleton().erase_all();
        }
        AccumulatorClient::singleton().finalize();
        _mc->barrier("XXXXX", _num);
        if (comm_rank() == 0) {
            AccumulatorServer::singleton().finalize();
        }
        _rpc->finalize();
        _mc->finalize();

        p.reset();
        _master->exit();
        _master->finalize();
    }

    comm_rank_t comm_rank() {
        return _rpc->global_rank();
    }

    comm_rank_t comm_size() {
        return _num;
    }

    void wait_empty() {
        AccumulatorClient::singleton().wait_empty();
        AccumulatorServer::singleton().wait_empty();
        _mc->barrier("XXXX", _num);
    }

    size_t _num;
    std::unique_ptr<Master> _master;
    std::unique_ptr<TcpMasterClient> _mc;
    std::unique_ptr<RpcService> _rpc;
    std::unique_ptr<MultiProcess> p;
};

void AccumulatorTest_single_thread_per_rank_sum_int_check_ok(size_t num) {
    AccumulatorMultiProcess mp(num);
    Accumulator<SumAggregator<int64_t>> counter("sum_int_counter_single_ok", 10);
    const int count_max = 1000;
    for (int i = 0; i < count_max; i++) {
        ASSERT_TRUE(counter.write((i+1) * (mp.comm_rank()+1)));
    }
    mp.wait_empty();
    if (mp.comm_rank() == 0) {
        Accumulator<SumAggregator<int64_t>> counter("sum_int_counter_single_ok");
        std::string cnt_res;
        ASSERT_TRUE(counter.try_read_to_string(cnt_res));
        std::string right_res = boost::lexical_cast<std::string>((1+count_max)*count_max/2
                * mp.comm_size() * (mp.comm_size() + 1) / 2);
        EXPECT_STREQ(right_res.c_str(), cnt_res.c_str());
    }
}

void AccumulatorTest_multi_thread_per_rank_sum_int_check_ok(size_t num) {
    AccumulatorMultiProcess mp(num);
    const int64_t thread_num = 101;
    const int64_t count_max = 1000;
    std::thread thr[thread_num];
    for (int64_t tid = 0; tid < thread_num; tid++) {
        thr[tid] = std::thread([&mp, tid]() {
                Accumulator<SumAggregator<int64_t>> counter("sum_int_counter_multi_ok", 10);
                for (int64_t i = 0; i < count_max; i++) {
                    ASSERT_TRUE(counter.write((i+1) * (tid+1) * (mp.comm_rank()+1)));
                }
            });
    }
    for (int64_t tid = 0; tid < thread_num; tid++) {
        thr[tid].join();
    }
    
    mp.wait_empty();
    if (mp.comm_rank() == 0) {
        for (int64_t i = 0; i < mp.comm_size(); i++) {
            Accumulator<SumAggregator<int64_t>> counter("sum_int_counter_multi_ok");
            std::string cnt_res;
            ASSERT_TRUE(counter.try_read_to_string(cnt_res));
            std::string right_res = boost::lexical_cast<std::string>( (1+count_max)*count_max/2
                    * thread_num * (thread_num + 1) / 2
                    * mp.comm_size() * (mp.comm_size() + 1) / 2);
            EXPECT_STREQ(right_res.c_str(), cnt_res.c_str());
        }
    }
}

void AccumulatorTest_single_thread_per_rank_maxmin_int_check_ok(size_t num) { 
    AccumulatorMultiProcess mp(num);  
    Accumulator<ArithmeticMaxAggregator<int>> counter_max("max_int_counter_single_ok", 10);
    Accumulator<ArithmeticMinAggregator<int>> counter_min("min_int_counter_single_ok", 10);
    const int64_t count_max = 10000;
    for (int64_t i = 0; i < count_max; i++) {
        ASSERT_TRUE(counter_max.write((i+1) * (mp.comm_rank()+1)));
        ASSERT_TRUE(counter_min.write((i+1) * (mp.comm_rank()+1)));
    }
    
    mp.wait_empty();
    Accumulator<AvgAggregator<float>> dummy1("dummy1");
    Accumulator<TimerAggregator<float>> dummy2("dummy2");

    if (mp.comm_rank() == 0) {
        Accumulator<ArithmeticMaxAggregator<int>> counter_max("max_int_counter_single_ok", 10);
        Accumulator<ArithmeticMinAggregator<int>> counter_min("min_int_counter_single_ok", 10);
        std::string cnt_res;
        ASSERT_TRUE(counter_max.try_read_to_string(cnt_res));
        std::string right_res = boost::lexical_cast<std::string>(count_max * mp.comm_size());
        EXPECT_STREQ(right_res.c_str(), cnt_res.c_str());
        
        ASSERT_TRUE(counter_min.try_read_to_string(cnt_res));
        right_res = boost::lexical_cast<std::string>(1);
        EXPECT_STREQ(right_res.c_str(), cnt_res.c_str());
    }
}

static constexpr size_t REPEAT_TIME = 10;
std::vector<size_t> NUM_PROCESS = {1, 3, 5, 8};

TEST(AccumulatorTest, single_thread_per_rank_sum_int_check_ok) {
    for (size_t i = 0; i < REPEAT_TIME; ++i) {
        for (size_t np: NUM_PROCESS) {
            AccumulatorTest_single_thread_per_rank_sum_int_check_ok(np);
        }
    }
}

TEST(AccumulatorTest, multi_thread_per_rank_sum_int_check_ok) {
    for (size_t i = 0; i < REPEAT_TIME; ++i) {
        for (size_t np: NUM_PROCESS) {
            AccumulatorTest_multi_thread_per_rank_sum_int_check_ok(np);
        }
    }
}

TEST(AccumulatorTest, single_thread_per_rank_maxmin_int_check_ok) {   
    for (size_t i = 0; i < REPEAT_TIME; ++i) {
        for (size_t np: NUM_PROCESS) {
            AccumulatorTest_single_thread_per_rank_maxmin_int_check_ok(np);
        }
    }
}

}
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
