FROM ubuntu:18.04
RUN apt-get update && apt-get install -y g++-7  openssl curl wget git \
    autoconf cmake protobuf-compiler protobuf-c-compiler zookeeper zookeeperd googletest build-essential libtool libsysfs-dev pkg-config
RUN apt-get update && apt-get install -y libsnappy-dev libprotobuf-dev libprotoc-dev libleveldb-dev \
    zlib1g-dev liblz4-dev libssl-dev libzookeeper-mt-dev libffi-dev libbz2-dev liblzma-dev

ENV TENSORFLOW=2.2
ENV HOROVOD_GPU_ALLREDUCE=NCCL

ADD . /pico-core
ENV GITHOST=hub.fastgit.org
RUN bash /pico-core/third_party/prepare.sh build gflags glog googletest sparsehash zlib snappy lz4 boost yaml jemalloc prometheus-cpp avro-cpp brpc
WORKDIR /pico-core
RUN mkdir build && cmake .. && make -j && make test && make install && cd ..
