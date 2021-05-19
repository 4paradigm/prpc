#!/bin/bash
source ${install_script_dir}/common.sh


CFLAGS=-fPIC \
    CXXFLAGS=-fPIC \
    LDFLAGS=-fPIC \
    execshell "install_configure_pkg zlib"

