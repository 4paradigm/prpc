## Pico Core

### Pico Core简介

Pico Core是一个函数库，主要为高性能计算提供网络通讯，累加器，安全线程等功能。[详情](src/README.md)

### Pico Core编译

#### (推荐)使用Docker进行编译

```
docker build . -t {your_image_name}
```

#### Ubuntu


```
apt-get update && apt-get install -y g++-7  openssl curl wget git \
autoconf cmake protobuf-compiler protobuf-c-compiler zookeeper zookeeperd googletest build-essential libtool libsysfs-dev pkg-config
apt-get install -y libsnappy-dev libprotobuf-dev libprotoc-dev libleveldb-dev \
    zlib1g-dev liblz4-dev libssl-dev libzookeeper-mt-dev libffi-dev libbz2-dev liblzma-dev
cmake .
make
```



