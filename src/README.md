## Pico core 源码目录

## 源码结构

### Accumulator 

    核心功能，Pico累加器，[使用说明](Accumulator/README.md)，[设计文档](Accumulator/Design.md)

### rpc

    核心功能，Pico通讯框架，[使用说明](rpc/README.md)，[设计文档](rpc/Design.md)

### addition

    附加功能，为核心功能模块，提供哈希，Json格式转化等功能

### codec

    编码功能，主要提供URI与Base64编码支持

### common

    通用功能，为核心模块提供序列化，类型转换，反射，日志，监控等功能

### thread

    线程模块，主要为两个核心模块提供线程安全方面的支持，内容包括多进程单客户端（Mpsc)消息队列，Rpc Channel，信号量，回旋锁等