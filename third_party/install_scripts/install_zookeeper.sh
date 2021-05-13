#!/bin/bash
source ${install_script_dir}/common.sh

pkg_patch_list="${patches_dir}/patch-zookeeper-fix_lock_bug.patch ${patches_dir}/patch-zookeeper-fix_envlog.patch ${patches_dir}/patch-zookeeper-fix_gcc9_error.patch" \
configure_preprocess="pushd ./src/c && patch -f -p1 < ${patches_dir}/patch-zookeeper-fix_lock_bug.patch && patch -f -p1 < ${patches_dir}/patch-zookeeper-fix_envlog.patch && patch -f -p1 < ${patches_dir}/patch-zookeeper-fix_gcc9_error.patch" \
    install_cmd="make install && popd" \
    configure_flags="--without-cppunit --with-pic" \
    execshell "install_configure_pkg zookeeper"

