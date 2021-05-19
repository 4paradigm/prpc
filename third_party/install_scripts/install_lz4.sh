#!/bin/bash
source ${install_script_dir}/common.sh

CFLAGS=-fPIC \
    CXXFLAGS=-fPIC \
    LDFLAGS=-fPIC \
    install_cmd="make install" \
    execshell "install_make_pkg lz4"
