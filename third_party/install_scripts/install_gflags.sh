#!/bin/bash
source ${install_script_dir}/common.sh


cmake_exdefine="-DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_C_FLAGS=-fPIC -DBUILD_SHARED_LIBS=1 -DBUILD_STATIC_LIBS=1" \
    execshell "install_cmake_pkg gflags"
