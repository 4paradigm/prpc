#!/bin/bash
source ${install_script_dir}/common.sh

pkg_patch_list="${patches_dir}/patch-jemalloc.patch" \
configure_preprocess="patch -f -p0 < ${pkg_patch_list} && bash autogen.sh" \
    install_cmd="make install_bin install_include install_lib" \
    configure_flags="--with-jemalloc-prefix=je_ --disable-cxx --disable-stats --disable-initial-exec-tls --with-version=5.1.0-1-g0 " \
    execshell "install_configure_pkg jemalloc"
