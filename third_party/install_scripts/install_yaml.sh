#!/bin/bash
source ${install_script_dir}/common.sh

function install() {
    # build both static and shared
    local set_prefix=""
    if [ "X$prefix" != "X" ]; then
        set_prefix="-DBoost_INCLUDE_DIR=$prefix/include"
    fi
    local cmake_exdefine_common="-DYAML_CPP_BUILD_TOOLS=OFF 
        -DCMAKE_CXX_FLAGS=-fPIC 
        -DCMAKE_C_FLAGS=-fPIC
        $set_prefix"
    cmake_exdefine="${cmake_exdefine_common}" \
        execshell "inner_install_cmake_pkg yaml"
    cmake_exdefine="${cmake_exdefine_common} -DBUILD_SHARED_LIBS=ON" \
        execshell "inner_install_cmake_pkg yaml"
}

execshell "install_pkg install yaml"

