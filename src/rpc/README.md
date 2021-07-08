# RPC 介绍

## 目录

- [RPC 介绍](#rpc-介绍)
  - [目录](#目录)
  - [简介](#简介)
  - [简单示例](#简单示例)
    - [Server.cpp](#servercpp)
    - [Client.cpp](#clientcpp)

## 简介

prpc 是一个专注于高性能计算的 client-server 通讯框架，与 socket 相比，它具有线程安全，兼容 RDMA，低响应时间，兼容多种数据类型，支持多对多，自动重传，无需关心连接状态等优势。

## 简单示例

以一个最简单的 client-server 通讯为例，[完整代码](../test/rpc_test.cpp)。

### Server.cpp

首先创建一个 master，一个 master 即为一个 namespace，维护着所有 server 和 client 的全局信息，server 端需要向对应的 master 进行注册，client 端连接对应的 master 后可以访问所有向其注册过的 server。

```
    Master master("127.0.0.1:9394");
    master.initialize();
```

随后，使用该 master 来初始化通讯框架。

```
    TcpMasterClient master_client(master.endpoint());
    master_client.initialize();

    RpcService rpc;
    RpcConfig rpc_config;
    rpc_config.bind_ip = "127.0.0.1";
    rpc.initialize(&master_client, rpc_config);
```

使用该框架来生成 server 端，注意这里的 “asdf” 是 server 的 id，client 连接时通过这个 id 来指定连接的 server。

```
    std::unique_ptr<RpcServer> server = rpc.create_server("asdf");
```

使用生成的 server 来创建 dealer，dealer 为收发消息的接口类。

```
    std::shared_ptr<Dealer> s_dealer = server->create_dealer();
```

最后使用 dealer 来进行消息的收发。

```
    RpcRequest req;
    std::string s;
    s_dealer->recv_request(req);
    RpcResponse resp(req);
    req >> s;
    resp << s;
    s_dealer->send_response(std::move(resp))
```

完成通讯后，需要依次析构。

```
    s_dealer.reset();
    server.reset();
    rpc.finalize();
    master_client.clear_master();
    master_client.finalize();
    master.exit();
    master.finalize();
```

### Client.cpp

首先连接 server 端所在的 master ，并用它来初始化通讯框架,这里的 "ip:port" 必须与 Server 端创建的 master 相同。

```c++
    TcpMasterClient master_client("127.0.0.1:9394");
    master_client.initialize();
    SLOG(INFO) << "Client initialized.";

    RpcService rpc;
    RpcConfig rpc_config;
    rpc_config.bind_ip = "127.0.0.1";
    rpc.initialize(&master_client, rpc_config);
```

随后使用该通讯框架来初始化 client ，这里的 "asdf" 为 server 创建时指定的 id，参数 1 为该 server id 上启动服务的数量，在多对多的场合，通过正确指定该数值可以进行负载均衡。

```c++
    std::unique_ptr<RpcClient> client = rpc.create_client("asdf", 1);
```

创建并初始化dealer。

```c++
    auto c_dealer = client->create_dealer();
```

通过该 dealer 来收发数据。

```c++
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

prpc 支持服务的热插拔功能，若当前dealer所连接的服务下线或者出现网络异常无法连接，则将 dealer 重置后可以连接其它 service id 相同的服务，无需重新创建链接。

```c++
    if (e == RpcErrorCodeType::ENOSUCHSERVER){
        c_dealer.reset();
        c_dealer->send_request(std::move(req));
        bool ret = c_dealer->recv_response(resp);
        SCHECK(ret);
        resp >> e;
        SCHECK(s == e);
    }
```

发送完成后进行析构。

```c++
    c_dealer.reset();
    client.reset();
    rpc.finalize();
    master_client.clear_master();
    master_client.finalize();
```