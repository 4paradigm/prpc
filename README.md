[![build status](https://github.com/4paradigm/prpc/actions/workflows/build.yml/badge.svg)](https://github.com/4paradigm/prpc/actions/workflows/build.yml)
[![docker pulls](https://img.shields.io/docker/pulls/4pdosc/prpc.svg)](https://hub.docker.com/r/4pdosc/prpc)

## About

prpc is an RPC framework that provides network communication for high-performance computing, with components such as accumulator.

## Build

### Docker Build

```
docker build -t 4pdosc/prpc-base:latest -f docker/Dockerfile.base .
docker build -t 4pdosc/prpc:0.0.0 -f docker/Dockerfile .
```

### Ubuntu

```
apt-get update && apt-get install -y g++-7  openssl curl wget git \
autoconf cmake protobuf-compiler protobuf-c-compiler zookeeper zookeeperd googletest build-essential libtool libsysfs-dev pkg-config
apt-get install -y libsnappy-dev libprotobuf-dev libprotoc-dev libleveldb-dev \
    zlib1g-dev liblz4-dev libssl-dev libzookeeper-mt-dev libffi-dev libbz2-dev liblzma-dev
mkdir build && cd build && cmake .. && make -j && make install && cd ..
```

## Design

### Client

![](src/rpc/img/Client.png)

1. Initialize `RpcService` and register on `Master`.
2. The `Master` receives the registration request and returns the global `RpcService` information, including the number, address, and service registered on the node.
3. `RpcService` creates `FrontEnd` for each service node based on the returned information.
4. `FrontEnd` will only connect to the target node when the information needs to be sent, and manage the connection to ensure the reliability of the service.
5. If the server is also on this node, the message can be moved to the server directly, otherwise it needs to go through TCP or RDMA network.
6. Send the message to the target service node.

### Server

![](src/rpc/img/Server.png)

1. Initialize `RpcService` and register on `Master`.
2. After receiving the registration request, the `Master` will return a confirmation message.
3. Then `Master` will broadcast the node's rank, address and services to all nodes.
4. `RpcService` creates and continues to listen the acceptor fd.
5. Other nodes connect to this node.
6. For each new listened connection, a new connection fd will be established to send and receive data according to the node configuration and network topology.
7. Other nodes send messages to this node through TCP, RDMA protocols.
8. After the connection fd receives the message, the background receiving thread will be notified and the message will be filled into `RpcChannel`.
9. Get the message from `RpcChannel` through `recv_request()`.
10. Complete message reception.

### FrontEnd

In order to optimize response time and communication efficiency, a non-blocking mode is adopted for message sending, and the specific implementation is as follows:

![](src/rpc/img/frontend1.png)

`FrontEnd` uses a thread-safe buffer (multiple producers and single consumer). When a thread (Thread1) sends a message, it first pushes the message into the buffer. If there is no other messages in the buffe, the thread will send all messages pushed to the buffer until the buffer is cleared. This thread is called working thread.

![](src/rpc/img/frontend2.png)

When an other thread (Thread2) sends a message, after its push message enters the buffer, if it detects that the corresponding buffer already has a worker thread (Thread1), it can return directly, and the message is sent by the worker thread.

![](src/rpc/img/frontend3.png)

Since the non-blocking system call is used when sending data, if a message is too large to be completely sent in one system call, in order to ensure response time, the worker thread will create a temporary thread to send the remaining content, and continue to process the next message in buffer.

In this way, thread safety is ensured when messages are sent by multiple threads, context switching, memory copying are minimized, and CPU cache misses are avoided as much as possible.

### Exception

The exception handling process of client‘s `send_request` is shown in the following figure:

![](src/rpc/img/frontend4.png)

When the sending of `FrontEnd` fails due to network or other reasons, it will set its current status to EPIPE and try to find an other `FrontEnd` registered with the same service. If found, the message will be forward to it, otherwise an error response will be returned immediately.


## More Documents

[Overview](src/README.md)

[RPC Tutorial](src/rpc/README.md)

[Accumulator Tutorial](src/accumulator/README.md)

[Accumulator Design](src/accumulator/Design.md)
