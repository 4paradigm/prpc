#!/bin/bash
source ${install_script_dir}/common.sh


configure_flags="CXXFLAGS=-std=c++11" \
    execshell "install_configure_pkg sparsehash"
