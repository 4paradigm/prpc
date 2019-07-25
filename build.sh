#!/bin/bash
set -e -x
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
    export zk_recv_timeout='1000'
    export zk_disconnect_timeout='1000'
    export zk_root_path='ut'
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

function ut() {
    mkdir -p ${PROJECT_ROOT}/.ut
    pushd ${PROJECT_ROOT}/.ut
    #cp -r ${THIRD_PARTY_PREFIX}/zookeeper .
    mkdir -p ${PROJECT_ROOT}/.ut/zk_data
    cat > ${PROJECT_ROOT}/.ut/zoo.cfg <<EOF
tickTime=2000
initLimit=10
syncLimit=5
dataDir=${PROJECT_ROOT}/.ut/zk_data/
clientPort=54321
server.1=127.0.0.1:42330:43330
EOF
    ${THIRD_PARTY_PREFIX}/zookeeper/bin/zkServer.sh start ${PROJECT_ROOT}/.ut/zoo.cfg

    for ((i=1;i>0;i++)); do
        ${THIRD_PARTY_PREFIX}/zookeeper/bin/zkServer.sh status ${PROJECT_ROOT}/.ut/zoo.cfg
        if [ $? -eq 0 ]; then
            break
        fi
        sleep 5
    done
    export zk_endpoint='127.0.0.1:54321'
    tests=`find /home/yiming/pico/pico-core/build/ -path *_test`
    for i in $tests; do
        $i
    done
    #tests=`find ${PROJECT_ROOT}/build/ -path *_test`
    ${THIRD_PARTY_PREFIX}/zookeeper/bin/zkServer.sh stop ${PROJECT_ROOT}/.ut/zoo.cfg
    popd
    rm -rf ${PROJECT_ROOT}/.ut 
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
if [ "$1" == "ut" ];then
    build
    ut
else
    build $*
fi
publish

