root_dir=`pwd`
./third-party/prepare.sh build cmake glog gflags yaml boost zookeeper zlib snappy lz4  jemalloc sparsehash googletest libunwind
mkdir -p build
pushd build
cmake -DCMAKE_MODULE_PATH=${root_dir}/cmake ..
make
popd
