[TOC]

## Pico 通讯框架

## 简介

### Pico 通讯框架简介

Pico通讯框架是一个专注于高性能计算的client-server通讯框架，与传统的socket相比，它具有线程安全，兼容rDMA，低响应时间，兼容多种数据类型，可以支持多对多发送等优势

### Pico 通讯框架的使用

以一个最简单的client-server通讯为例：

Server.cc：

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
