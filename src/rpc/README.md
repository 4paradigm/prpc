[TOC]

## Pico 通讯框架

## 简介

### Pico 通讯框架简介

Pico通讯框架是一个专注于高性能计算的client-server通讯框架，与传统的socket相比，它具有线程安全，兼容rDMA，低响应时间，兼容多种数据类型，支持多对多，自动重传，无需关心连接状态等优势

### Pico 设计文档

[设计](Design.md)

### Pico 通讯框架的使用

以一个最简单的client-server通讯为例，[完整代码](../test/rpc_test.cpp)

#### Server.cpp：

首先创建一个master，一个master即为一个namespace，维护着所有server和client的全局信息，server端需要向对应的master进行注册，client端连接对应的master后可以访问所有向其注册过的server

```
    Master master("127.0.0.1");
    master.initialize();
```

随后，使用该master来初始化通讯框架

```
    TcpMasterClient master_client(master.endpoint());
    master_client.initialize();

    RpcService rpc;
    RpcConfig rpc_config;
    rpc_config.bind_ip = "127.0.0.1";
    rpc.initialize(&master_client, rpc_config);
```

使用该框架来生成server端，注意这里的“asdf”是server的id，client连接时通过这个id来指定连接的server

```
    std::unique_ptr<RpcServer> server = rpc.create_server("asdf");
```

使用生成的server来创建dealer，dealer为收发消息的接口类

```
    std::shared_ptr<Dealer> s_dealer = server->create_dealer();
```

最后使用dealer来进行消息的收发

```
    RpcRequest req;
    std::string s;
    s_dealer->recv_request(req);
    RpcResponse resp(req);
    req >> s;
    resp << s;
    s_dealer->send_response(std::move(resp))
```

完成通讯后，需要依次析构

```
    s_dealer.reset();
    server.reset();
    rpc.finalize();
    master_client.clear_master();
    master_client.finalize();
    master.exit();
    master.finalize();
```

#### Client.cpp:

首先连接server端所在的master,并用它来初始化通讯框架

```
    TcpMasterClient master_client(master.endpoint());
    master_client.initialize();
    SLOG(INFO) << "Client initialized.";

    RpcService rpc;
    RpcConfig rpc_config;
    rpc_config.bind_ip = "127.0.0.1";
    rpc.initialize(&master_client, rpc_config);
```

随后使用该通讯框架来初始化client，这里的"asdf"为server创建时指定的id，参数1为该server id上启动服务的数量，在多对多的场合，通过正确指定该数值可以进行负载均衡

```
    std::unique_ptr<RpcClient> client = rpc.create_client("asdf", 1);
```

创建并初始化dealer

```
    auto c_dealer = client->create_dealer();
```

通过该dealer来收发数据

```
    RpcRequest req;
    RpcResponse resp;
    std::string s = "asdfasdfasdfasf", e;
    req << s;
    c_dealer->send_request(std::move(req));
    bool ret = c_dealer->recv_response(resp);
    SCHECK(ret);
    resp >> e;
    SCHECK(s == e);
```

发送完成后进行析构

```
    c_dealer.reset();
    client.reset();
    rpc.finalize();
    master_client.clear_master();
    master_client.finalize();
```