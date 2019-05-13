#!/bin/bash
set -x
PROJECT_ROOT=`pwd`
echo ${PROJECT_ROOT}
#THIRD_PARTY_PREFIX=?
#THIRD_PARTY_SRC=?
#PREFIX=?
#USE_RDMA=?
#J=?
#PATH
#LD_LIBRARY_PATH

function usage() {
    echo "haha"
}

function setup() {
    if [ 0"${THIRD_PARTY_SRC}" == "0" ]; then
        THIRD_PARTY_SRC=${PROJECT_ROOT}/third-party
    fi
    if [ 0"${THIRD_PARTY_PREFIX}" == "0" ]; then
        THIRD_PARTY_PREFIX=${PROJECT_ROOT}/third-party
    fi
    # install tools
    prefix=${THIRD_PARTY_PREFIX} ${THIRD_PARTY_SRC}/prepare.sh build cmake glog gflags yaml boost zookeeper zlib snappy lz4  jemalloc sparsehash googletest libunwind
    if [ "${WITH_RDMA}" == "1" ];then
        prefix=${THIRD_PARTY_PREFIX} ${THIRD_PARTY_SRC}/prepare.sh build rdma-core
    fi
}

function build() {
    if [ "${USE_RDMA}" == "1" ];then
        EXTRA_DEFINE="${EXTRA_DEFINE} -DUSE_RDMA=ON"
    else
        EXTRA_DEFINE="${EXTRA_DEFINE} -DUSE_RDMA=OFF"
    fi
    mkdir -p ${PROJECT_ROOT}/build
    pushd ${PROJECT_ROOT}/build
    cmake -DCMAKE_MODULE_PATH=${PROJECT_ROOT}/cmake ${EXTRA_DEFINE} ..
    make -j${J}
    popd
}

setup
build


