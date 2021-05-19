#!/bin/bash
source ${install_script_dir}/common.sh

configure_preprocess="cp config configure"
configure_predef="CPPFLAGS=\"-I$prefix/include -fPIC\"  LDFLAGS=-L$prefix/lib " \
configure_flags="--openssldir=${prefix} -shared" \
install_cmd="make install_sw" \
execshell "install_configure_pkg openssl"

