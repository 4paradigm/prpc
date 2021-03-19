#!/bin/bash
source ${install_script_dir}/common.sh

function install_boost() {
    local set_prefix=""
    if [ "X$prefix" != "X" ]; then
        set_prefix="--prefix=$prefix"
    fi
    execshell "./bootstrap.sh ${set_prefix} --without-libraries=python"
    boost_opt="-s NO_BZIP2=1 -s NO_ZLIB=1"

    local set_J="-j`cat /proc/cpuinfo| grep "processor"| wc -l`"
    if [ "X$J" != "X" ]; then
        set_J="-j$J"
    fi
    execshell "./bjam ${set_prefix} $boost_opt cxxflags=\"-fPIC -std=c++11\" cflags=-fPIC --without-python --link=static --runtime-link=static ${set_J}"
    execshell "./b2 install ${set_J}"
}

execshell "install_pkg install_boost boost"

