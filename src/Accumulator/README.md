[TOC]

## Pico 累加器

## 简介

### Pico 累加器简介

Pico累加器是一个汇总并处理分布式数据的功能模块。用户可以多进程，多节点的向Pico累加器上推送数据，累加器将收集所有推送的数据，进行处理后以供查询。Pico累加器对于数据的处理不仅包括累加，还包括求平均，最大值，最小值等。具体见下文所述

### Pico 累加器支持的数据处理

Pico累加器支持的数据处理及其所对应的类型名称如下表所述：

[表格]

### Pico 累加器的使用

我们以多节点使用具有数据累加功能的Pico累加器为例，对于其中任何一个节点，在使用Pico累加器之前，都必须先创建Pico通讯框架，关于Pico通讯框架的使用方法，可以点击[这里](../rpc/README.md)进行了解

```
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

随后，选择一个节点作为累加器的服务节点，服务节点需要负责接收其它节点的数据，并将其处理后保存在本地。这里我们选择第一个初始化成功Pico通讯框架的节点为服务节点，其它节点为客户端节点，需要向服务节点推送数据

```
    if (_rpc->global_rank() == 0) {
        AccumulatorServer::singleton().initialize(_rpc.get());
    }
    AccumulatorClient::singleton().initialize(_rpc.get());
```

在确定了累加器的服务节点之后，所有节点须创建并指定对应的累加器类型，如累加，平均，最大最小值等。这里"sum_int_count_single_ok"为累加器ID，累加器的客户端的ID必须与服务端ID相同才能推送数据，SumAggregator<int64_t>为累加器类型，所有的客户端与服务端必须相同，参数10为刷新频率，越低表示刷新的越快，同时网络负载也越大。

```
    Accumulator<SumAggregator<int64_t>> counter("sum_int_counter_single_ok", 10)
```

现在我们可以推送数据了，在这个例子中，每个客户端节点须推送1000次数据，每次数据为当前次数与节点编号的乘积，并等待数据发送完毕

```
    const int count_max = 1000;
    for (int i = 0; i < count_max; i++) {
        ASSERT_TRUE(counter.write((i+1) * (_rpc.global_rank()+1)));
    }
    AccumulatorClient::singleton().wait_empty();
```

当所有的节点都发送完毕后，可以获取累加器的最终结果，并确认其数值是否正确

```
    ASSERT_TRUE(counter.try_read_to_string(cnt_res));
    std::string right_res = boost::lexical_cast<std::string>((1+count_max)*count_max/2
            * _rpc.global_rank() * (_rpc.global_rank() + 1) / 2);
    EXPECT_STREQ(right_res.c_str(), cnt_res.c_str());
```

使用完毕后，需要对相关资源进行释放

```
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