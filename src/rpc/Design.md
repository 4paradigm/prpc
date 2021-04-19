[TOC]

## 名次解释

* Master

    一个全局服务，维护着各个节点(包括client与server)的状态，每个节点需要向其注册才能使用

* RpcService

    一个逻辑节点，该节点上可以启动多个Server与Client，每个节点有唯一的节点id

* RpcChannel

    从多个通讯渠道（socket,rDMA)中读取数据，并推送进缓冲区，从而保证通讯效率与线程安全

## 整体流程框架

### Client端

![](img/client.png)

1. 创建新的RpcService并向master注册
2. Master收到注册请求，并返回全局的RpcService信息，包括节点数量，节点名称，以及每个节点上注册的服务数量和地址等信息
3. RpcService将这些信息接受后，与自身要连接的server信息相比对，并将这些信息传递给FrontEnd Layer层进行后续的连接发动等操作
4. FrontEnd与目标节点进行惰性连接，只有在信息需要发送过去的场合才会与目标节点进行握手，并管理该连接，保证服务的可靠性
5. FrontEnd根据节点设置与网络拓扑，决定底层的传输协议，若服务端也在本节点的话，可以直接走RPC Channel将消息发送到服务端，否则需要经过socket,rDMA等将消息发送到对方节点上去。
6. 将消息通过对应的传输协议传输到目标节点

### Server端

1. 


