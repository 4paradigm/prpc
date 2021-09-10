# Accumulator Design

English version | [中文版](Design_cn.md)

## Table of Contents

- [Accumulator Design](#accumulator-design)
  - [Table of Contents](#table-of-contents)
  - [Accumulator Write](#accumulator-write)
  - [Accumulator Read](#accumulator-read)

## Accumulator Write 

![](img/Acwrite.png)

1. Send the data to the local cache.
2. `Accumulator` uses `Aggregator` to aggregate local data, and periodically flushed to `AccumulatorClient`.
3. The `AccumulatorClient` has two `channel`s, which receive data in turn according to the double buffering strategy.
4. When one channel is receiving data, the other is sending data to the server.
5. Message passing through prpc.
6. `AccumulatorServer` receives the data pushed by all nodes.
7. `AccumulatorServer` uses `Aggregator` to aggregate all received data and save the results for query.

## Accumulator Read

![](img/Acread.png)

1. `Accumulator` forwards to `AccumulatorClient`.
2. `AccumulatorClient` sends a request to `AccumulatorServer`.
3. Message passing through prpc.
4. Send the request to `AccumulatorServer` for processing.
5. Complete the corresponding query operation according to the `Aggregator`.
