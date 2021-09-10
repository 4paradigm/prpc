# Accumulator 设计

[English version](Design.md) | 中文版

## Table of Contents

- [Accumulator 设计](#accumulator-设计)
  - [Table of Contents](#table-of-contents)
  - [Accumulator Write](#accumulator-write)
  - [Accumulator Read](#accumulator-read)

## Accumulator Write

![](img/Acwrite.png)

1. 将数据发送给本地的缓存。
2. `Accumulator` 使用 `Aggregator` 初步聚合，并周期性刷新到 `AccumulatorClient`。
3. `AccumulatorClient` 有两个 `channel`，按照双缓冲策略轮流接收数据。
4. 当其中一个 channel 接收数据时，另一个则发送数据。
5. 通过 prpc 进行消息传递。
6. `AccumulatorServer` 接收所有节点推送的数据。
7. 使用 `Aggregator` 对聚合所有接收的数据，并将结果保存以供查询。

## Accumulator Read

![](img/Acread.png)

1. `Accumulator` 直接转发给 `AccumulatorClient`。
2. `AccumulatorClient` 发送请求给 `AccumulatorServer`。
3. 通过 prpc 进行消息传递。
4. 将请求送达到 `AccumulatorServer` 进行处理。
5. 根据 `Aggregator` 完成对应的查询操作。
