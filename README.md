[![build status](https://github.com/4paradigm/prpc/actions/workflows/build.yml/badge.svg)](https://github.com/4paradigm/prpc/actions/workflows/build.yml)
[![docker pulls](https://img.shields.io/docker/pulls/4pdosc/prpc.svg)](https://hub.docker.com/r/4pdosc/prpc)

## About

prpc is an RPC framework that provides network communication for high-performance computing, with components such as accumulator.

## Build

### Docker Build

```
docker build -t 4pdosc/prpc-base:0.1.0 -f docker/Dockerfile.base .
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

## Documents

[Overview](src/README.md)

[RPC Tutorial](src/rpc/README.md)

[RPC Design](src/rpc/Design.md)

[Accumulator Tutorial](src/accumulator/README.md)

[Accumulator Design](src/accumulator/Design.md)
