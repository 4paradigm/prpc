[![build status](https://github.com/4paradigm/prpc/actions/workflows/build.yml/badge.svg)](https://github.com/4paradigm/prpc/actions/workflows/build.yml)
[![docker pulls](https://img.shields.io/docker/pulls/4pdosc/prpc.svg)](https://hub.docker.com/r/4pdosc/prpc)
[![discuss](https://img.shields.io/badge/Discuss-Ask%20Questions-blue)](https://github.com/4paradigm/prpc/discussions)

## 关于

prpc 是一个为高性能计算提供网络通信的 RPC 框架，并附带 Accumulator 等组件。

## 编译

### 推荐使用 Docker 进行编译

```
docker build . -t {your_image_name}
```

### Ubuntu

```
apt-get update && apt-get install -y g++-7  openssl curl wget git \
autoconf cmake protobuf-compiler protobuf-c-compiler zookeeper zookeeperd googletest build-essential libtool libsysfs-dev pkg-config
apt-get install -y libsnappy-dev libprotobuf-dev libprotoc-dev libleveldb-dev \
    zlib1g-dev liblz4-dev libssl-dev libzookeeper-mt-dev libffi-dev libbz2-dev liblzma-dev
mkdir build && cd build && cmake .. && make -j && make install && cd ..
```

## 文档

[总体概览](src/README.md)

[RPC 简介](src/rpc/README.md)

[RPC 设计](src/rpc/Design.md)

[Accumulator 简介](src/accumulator/README.md)

[Accumulator 设计](src/accumulator/Design.md)
