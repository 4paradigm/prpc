#!/bin/bash
source ${install_script_dir}/common.sh

pkg_patch_list="${patches_dir}/patch-brpc-fix_cmake.patch" \
cmake_preprocess="patch -p1 -f < ${patches_dir}/patch-brpc-fix_cmake.patch" \
cmake_exdefine="-DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_C_FLAGS=-fPIC" \
    execshell "install_cmake_pkg brpc"
