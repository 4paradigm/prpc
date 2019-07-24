#!/bin/bash
set -e
PROJECT_ROOT=`pwd`
echo ${PROJECT_ROOT}
#THIRD_PARTY_PREFIX=
#THIRD_PARTY_SRC=
#PREFIX=?
#USE_RDMA=?
#J=?
#PATH=? 

function setup() {
    if [ 0"${THIRD_PARTY_SRC}" == "0" ]; then
        #git submodule update --init --recursive --checkout
        THIRD_PARTY_SRC=${PROJECT_ROOT}/third-party
    fi
    if [ 0"${THIRD_PARTY_PREFIX}" == "0" ]; then
        THIRD_PARTY_PREFIX=${PROJECT_ROOT}/third-party
    fi
    # install tools
    prefix=${THIRD_PARTY_PREFIX} ${THIRD_PARTY_SRC}/prepare.sh build cmake glog gflags yaml boost zookeeper zlib snappy lz4 jemalloc sparsehash googletest
    if [ "${USE_RDMA}" == "1" ];then
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
    ${THIRD_PARTY_PREFIX}/bin/cmake -DCMAKE_MODULE_PATH=${PROJECT_ROOT}/cmake -DTHIRD_PARTY=${THIRD_PARTY_PREFIX} -DCMAKE_INSTALL_PREFIX=${prefix} ${EXTRA_DEFINE} ..
    if [ 0"${J}" == "0" ];then
        J=`nproc | awk '{print int(($0 + 1)/ 2)}'` # make cocurrent thread number
    fi
    make -j${J} $*
    popd
}

function publish() {
    if [ 0"${prefix}" == "0" ]; then
        return 0
    fi
    pushd ${PROJECT_ROOT}/build
    make install
    popd
}

setup
build $*
publish

