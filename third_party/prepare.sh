#!/bin/bash
CURFILE=`readlink -m $0`
CURDIR=`dirname ${CURFILE}`
source ${CURDIR}/utils.sh

if [ "X$prefix" == "X" ]; then
    export prefix=/usr/local
else
    export PATH=${prefix}/bin:$PATH
    export LD_LIBRARY_PATH=${prefix}/lib:${prefix}/lib64:$LD_LIBRARY_PATH
    export PKG_CONFIG_PATH=${prefix}/lib/pkgconfig:${prefix}/lib64/pkgconfig:$PKG_CONFIG_PATH
fi
export CC=gcc
export CXX=g++
export CMAKE=cmake

export install_script_dir=${CURDIR}/install_scripts
export pkgs_dir=${CURDIR}/pkgs
export patches_dir=${CURDIR}/pkgs

function usage() {
cat  <<HELP_INFO
prepare.sh作用：
  bash prepare.sh build pkg1 [pkg2] ..      编译指定列表的包
  bash prepare.sh --help|-h                 显示此帮助信息
HELP_INFO
}

function build_pkgs() {
    if [ -f ${CURDIR}/env.sh ]; then
        source ${CURDIR}/env.sh
    fi
    execshell "mkdir -p ./tools"
    execshell "pushd ./tools"
    for pkg in $@; do
        execshell "bash ${install_script_dir}/install_${pkg}.sh"
    done
    execshell "popd"
}

case $1 in
    help|-h|-help|--help|--h)
        usage
    ;;

    build)
        shift
        build_pkgs "$@"
        log_notice "[\e[32;1mbuild SUCCESS\e[m]"
    ;;

    *)
        log_fatal "[\e[32;1munknown args:$@\e[m]"
    ;;
esac

exit 0
