# RPC

[English version](README.md) | 中文版

## 目录

- [RPC](#rpc)
  - [目录](#目录)
  - [功能特征](#功能特征)
  - [快速入门](#快速入门)
    - [Server.cpp](#servercpp)
    - [Client.cpp](#clientcpp)

## 功能特征

* 消息完整
* 线程安全
* 连接复用
* 自动重试
* 非阻塞
* RDMA
* 零拷贝
* 服务发现

## 快速入门

以一个简单的 client-server 通讯示例，

### Server.cpp

首先创建一个 master。 master 是一个命名空间，它维护着所有服务器和客户端的全局信息。 服务器需要在 master 上注册，客户端连接到 master 后可以访问所有注册的服务器。
```c++
    Master master("127.0.0.1:9394");
    master.initialize();
```

随后，使用该 master 的网络地址来初始化 prpc 框架。
```c++
    TcpMasterClient master_client(master.endpoint());
    master_client.initialize();

    RpcService rpc;
    RpcConfig rpc_config;
    rpc_config.bind_ip = "127.0.0.1";
    rpc.initialize(&master_client, rpc_config);
```

之后创建一个 server，注意这里的 “asdf” 是服务名称，client 连接时需要指定对服务名称。
```c++
    std::unique_ptr<RpcServer> server = rpc.create_server("asdf");
```

给这个 server 创建一个 `Dealer`，并使用 `Dealer` 来发送和接收消息，它不是线程安全的，但是 `create_dealer` 是线程安全的。
```c++
    std::shared_ptr<Dealer> s_dealer = server->create_dealer();

    RpcRequest req;
    std::string s;
    s_dealer->recv_request(req);
    RpcResponse resp(req);
    req >> s;
    resp << s;
    s_dealer->send_response(std::move(resp))
```

最后，依次析构所有对象。
```c++
    s_dealer.reset();
    server.reset();
    rpc.finalize();
    master_client.clear_master();
    master_client.finalize();
    master.exit();
    master.finalize();
```

### Client.cpp

首先连接服务器所在的 master。
```c++
    TcpMasterClient master_client("127.0.0.1:9394");
    master_client.initialize();
    SLOG(INFO) << "Client initialized.";

    RpcService rpc;
    RpcConfig rpc_config;
    rpc_config.bind_ip = "127.0.0.1";
    rpc.initialize(&master_client, rpc_config);
```

然后创建一个客户端，其中 “asdf” 是创建 server 时指定的服务名称，参数 1 是要等待的 server 数量。 
```c++
    std::unique_ptr<RpcClient> client = rpc.create_client("asdf", 1);
```

创建并初始化 `Dealer`。
```c++
    auto c_dealer = client->create_dealer();
```

发送请求并等待响应。
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

最后，依次析构所有对象。
```c++
    c_dealer.reset();
    client.reset();
    rpc.finalize();
    master_client.clear_master();
    master_client.finalize();
```