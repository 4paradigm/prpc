# RPC

English version | [中文版](README_cn.md)

## Table of Contents

- [RPC](#rpc)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
  - [Quick Start](#quick-start)
    - [Server](#server)
    - [Client](#client)

## Features

* Message complete
* Thread-safe
* Connection reuse
* Automatic retry
* Non-blocking
* RDMA
* Zero copy
* Service discovery

## Quick Start

The following is a simple client-server communication example.

### Server

First create a master. A master is a namespace, which maintains the global information of all servers and clients. The server needs to be registered on the master, and the client can access all the registered servers after connecting to the master.
```c++
    Master master("127.0.0.1:9394");
    master.initialize();
    master.finalize();
```

Then use the master endpoint to initialize the prpc framework.
```c++
    TcpMasterClient master_client(master.endpoint());
    master_client.initialize();

    RpcService rpc;
    RpcConfig rpc_config;
    rpc_config.bind_ip = "127.0.0.1";
    rpc.initialize(&master_client, rpc_config);
```

Then create a server. Note that "asdf" here is the service name of the server.
```c++
    std::unique_ptr<RpcServer> server = rpc.create_server("asdf");
```

Create a `Dealer` for the server. `Dealer` is used to send and receive messages and it is not thread-safe while `create_dealer` is thread-safe.
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

Finally, all objects are destructed in order.
```c++
    s_dealer.reset();
    server.reset();
    rpc.finalize();
    master_client.finalize();
    master.exit();
    master.finalize();
```

### Client

First connect to the master.
```c++
    TcpMasterClient master_client("127.0.0.1:9394");
    master_client.initialize();
    SLOG(INFO) << "Client initialized.";

    RpcService rpc;
    RpcConfig rpc_config;
    rpc_config.bind_ip = "127.0.0.1";
    rpc.initialize(&master_client, rpc_config);
```

Then create a client, where "asdf" is the service name specified when the server was created, and parameter 1 is the count of servers to wait.
```c++
    std::unique_ptr<RpcClient> client = rpc.create_client("asdf", 1);
    auto c_dealer = client->create_dealer();
```

Send a request and wait the response.
```c++
    RpcRequest req;
    RpcResponse resp;
    std::string s = "asdfasdfasdfasf", e;
    req << s;
    c_dealer->send_request(std::move(req));
    c_dealer->recv_response(resp);
    resp >> e;
```

Finally, all objects are destructed in order.
```c++
    c_dealer.reset();
    client.reset();
    rpc.finalize();
    master_client.finalize();
```
