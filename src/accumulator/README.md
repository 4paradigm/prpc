# Accumulator

English version | [中文版](README_cn.md)

## Table of Contents

- [Accumulator](#accumulator)
  - [Table of Contents](#table-of-contents)
  - [About](#about)
  - [Aggregator](#aggregator)
  - [Quick Start](#quick-start)

## About

`Accumulator` is a distributed component. Each node can push data, and `Accumulator` server aggregates all the pushed data for query. `Accumulator` supports custom aggregator while some aggregators are predifined.

## Aggregator

The predefined aggregators are as follows:
* SumAggregator
* AvgAggregator
* ArithmeticMaxAggregator
* ArithmeticMinAggregator

## Quick Start

The following is a simple distributed [example](../../test/accumulator_test.cpp).

Before using `Accumulator`, each node must [initialize prpc](../rpc/README_cm.md) firstly.
```c++
    _master = std::make_unique<Master>("127.0.0.1");
    _master->initialize();

    _mc = std::make_unique<TcpMasterClient>(_master->endpoint());
    _mc->initialize();
    RpcConfig rpc_config;
    rpc_config.protocol = "tcp";
    rpc_config.bind_ip = "127.0.0.1";
    rpc_config.io_thread_num = 1;
    _rpc = std::make_unique<RpcService>();
    _rpc->initialize(_mc.get(), rpc_config);
```

Then, select a node as the server node, and the server node needs to be responsible for receiving data from all nodes. Here, the first node that is successfully initialized is selected as the server node.
```c++
    if (_rpc->global_rank() == 0) {
        AccumulatorServer::singleton().initialize(_rpc.get());
    }
    AccumulatorClient::singleton().initialize(_rpc.get());
```

Then define the same `Accumulator` on all nodes. Here, "sum_int_count_single_ok" is the ID of the `Accumulator`. Same ID in different nodes will point to the same `Accumulator` entity. And `SumAggregator<int64_t>` is the aggregator, which must be strictly consistent when `Accumulator` defined on different nodes. Parameter 10 is the refresh frequency, which means that every 10 times `write` the client aggregates and pushes data to the server.

```c++
    Accumulator<SumAggregator<int64_t>> counter("sum_int_counter_single_ok", 10)
```

After that, each node pushes different data according to its rank.
```c++
    const int count_max = 1000;
    for (int i = 0; i < count_max; i++) {
        ASSERT_TRUE(counter.write((i+1) * (_rpc.global_rank()+1)));
    }
    AccumulatorClient::singleton().wait_empty();
```

After the data of all nodes have been pushed, you can get the summary and check it.
```c++
    ASSERT_TRUE(counter.try_read_to_string(cnt_res));
    std::string right_res = boost::lexical_cast<std::string>((1+count_max)*count_max/2
            * _rpc.global_rank() * (_rpc.global_rank() + 1) / 2);
    EXPECT_STREQ(right_res.c_str(), cnt_res.c_str());
```

Finally, the resources need to be released in sequence.
```c++
    if (_rpc.global_rank() == 0) {
        AccumulatorClient::singleton().erase_all();
    }
    AccumulatorClient::singleton().finalize();
    if (_rpc.global_rank() == 0) {
        AccumulatorServer::singleton().finalize();
    }
    _rpc->finalize();
    _master->exit();
    _master->finalize();
```