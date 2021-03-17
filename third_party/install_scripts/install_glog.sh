#!/bin/bash
source ${install_script_dir}/common.sh


configure_predef="CXXFLAGS=-fPIC CFLAGS=-fPIC" \
configure_preprocess="bash ./autogen.sh" \
    execshell "install_configure_pkg glog"


#cmake_exdefine="-DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_C_FLAGS=-fPIC -DBUILD_SHARED_LIBS=ON -DBUILD_SHARED_LIBS=ON" \
#    execshell "install_cmake_pkg glog"
