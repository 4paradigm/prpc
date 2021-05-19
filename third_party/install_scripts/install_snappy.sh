#!/bin/bash
source ${install_script_dir}/common.sh


CFLAGS=-fPIC \
    CXXFLAGS=-fPIC \
    LDFLAGS=-fPIC \
    configure_preprocess="bash ./autogen.sh" \
    execshell "install_configure_pkg snappy"

#execshell "rm -rf ${prefix}/lib/libsnappy.so"
