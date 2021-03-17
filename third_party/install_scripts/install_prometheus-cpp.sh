#!/bin/bash
source ${install_script_dir}/common.sh

function decompress_civetweb() {
    decompress civetweb
    local pkgdir=`find civetweb -mindepth 1 -maxdepth 1 -type d -name "*"`
    check_return "find civetweb -mindepth 1 -maxdepth 1 -type d -name '*' failed"
    if [ "X$pkgdir" = "X" ]; then
        log_fatal "cannnot find $1 related decompressed dir"
        exit 1
    fi
    execshell "mv $pkgdir/* 3rdparty/civetweb/"
}

function install() {
    # build both static and shared
    local cmake_exdefine_common="-DENABLE_PUSH=OFF 
    -DENABLE_TESTING=OFF 
    -DCMAKE_CXX_FLAGS=-fPIC 
    -DCMAKE_C_FLAGS=-fPIC 
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON"
    # static lib
    cmake_preprocess="decompress_civetweb" \
    cmake_exdefine="${cmake_exdefine_common}" \
        execshell "inner_install_cmake_pkg prometheus-cpp"
    # shared lib
    cmake_exdefine="${cmake_exdefine_common} -DBUILD_SHARED_LIBS=ON" \
        execshell "inner_install_cmake_pkg prometheus-cpp"
}

execshell "install_pkg install prometheus-cpp"

