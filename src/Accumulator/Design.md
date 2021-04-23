[TOC]

## 名词解释

* Accumulator Server

    每个Accumulator需要有一个Accumulator Server来收集数据与处理请求

* Accumulator Client

    向Accumulator Server上传数据与发送请求的客户端，可以与Accumulator Server部署在同一个节点

* RpcServer, RpcClient

    Pico 通讯框架的客户端与服务端，详情见[Pico通讯框架简介]（../rpc/README.md)

## Accumulator Write 流程 

![](img/Acwrite.png)

1. Accumulator 的写操作会将数据发送给本地的缓存
2. 与本地缓存已有的数据进行初步的整合后，定时进行Flush操作
3. Accumulator Client底层有两个channel，它们轮流负责接收Accumulator Client Flush的数据，与将数据发送给Rpc Client进行传输
4. 当其中一个channel负责接收数据时，另一个则负责发送数据，反之依然
5. 通过Pico通讯框架进行高性能通讯
6. Accumulator Server会负责接收其它节点推送的数据
7. 根据累加器的具体类型，进行数据的处理，并将结果保存以供查询

## Accumulator Read 流程

![](img/Acread.png)

1. Accumulator 的读操作会将请求发送给Accumulator Client
2. Accumulator Client 会将请求push到Rpc Client
3. 通过Pico 通讯框架进行高性能通讯
4. 将请求送达到Accumulator Server进行处理
5. 根据累加器具体的类型，Accumulator Server会进行对应的查询操作
