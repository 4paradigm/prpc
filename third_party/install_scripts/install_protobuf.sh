#!/bin/bash
source ${install_script_dir}/common.sh

function install() {
    # build both static and shared
    local cmake_exdefine_common="-DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_C_FLAGS=-fPIC
        -Dprotobuf_BUILD_TESTS=OFF
        -Dprotobuf_BUILD_EXAMPLES=OFF"
    cmake_exdefine="${cmake_exdefine_common}" \
        cmake_list_dir="cmake" \
        execshell "inner_install_cmake_pkg protobuf"
    cmake_exdefine="${cmake_exdefine_common} -Dprotobuf_BUILD_SHARED_LIBS=ON" \
        cmake_list_dir="cmake" \
        execshell "inner_install_cmake_pkg protobuf"
}

execshell "install_pkg install protobuf"
