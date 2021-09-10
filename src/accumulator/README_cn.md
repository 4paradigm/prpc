# Accumulator

[English version](README.md) | 中文版

## 目录

- [Accumulator](#accumulator)
  - [目录](#目录)
  - [简介](#简介)
  - [Aggregator](#aggregator)
  - [快速入门](#快速入门)

## 简介

`Accumulator` 是一个分布式组件。各节点可以推送数据，`Accumulator` 服务器聚合所有推送的数据以供查询。`Accumulator` 支持自定义聚合操作，目前已有一些预定义的聚合操作。

## Aggregator

预定义的聚合操作如下：
* SumAggregator
* AvgAggregator
* ArithmeticMaxAggregator
* ArithmeticMinAggregator

## 快速入门

以下是一个简单的分布式[示例](../../test/accumulator_test.cpp)

在使用 `Accumulator` 之前，每个节点都必须先[初始化 prpc](../rpc/README_cm.md)。
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

随后，选择一个节点作为服务器节点，服务器节点需要负责接收所有节点的数据。这里选择第一个初始化成功的节点为服务器节点。
```c++
    if (_rpc->global_rank() == 0) {
        AccumulatorServer::singleton().initialize(_rpc.get());
    }
    AccumulatorClient::singleton().initialize(_rpc.get());
```

然后，在所有节点上定义同一个 `Accumulator`。这里 "sum_int_count_single_ok" 是 `Accumulator` 的 ID，不同节点中相同的 ID 对应相同的 `Accumulator` 实体 ，`SumAggregator<int64_t>` 是聚合操作，一个 `Accumulator` 在所有节点中定义时都必须使用相同的聚合操作，参数 10 是刷新频率，表示每 10 次 `write` 客户端向服务器合并推送一次数据。
```c++
    Accumulator<SumAggregator<int64_t>> counter("sum_int_counter_single_ok", 10)
```

之后每个节点根据自己的编号推送不同的数据。
```c++
    const int count_max = 1000;
    for (int i = 0; i < count_max; i++) {
        ASSERT_TRUE(counter.write((i+1) * (_rpc.global_rank()+1)));
    }    
```

当所有节点的数据都推送完毕后，可以获取累加结果，并检查是否正确。
```c++
    AccumulatorClient::singleton().wait_empty();
    ASSERT_TRUE(counter.try_read_to_string(cnt_res));
    std::string right_res = boost::lexical_cast<std::string>((1+count_max)*count_max/2
            * _rpc.global_rank() * (_rpc.global_rank() + 1) / 2);
    EXPECT_STREQ(right_res.c_str(), cnt_res.c_str());
```

最后需要依次释放资源。
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