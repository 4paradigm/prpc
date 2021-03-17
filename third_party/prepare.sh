#!/bin/bash
CURFILE=`readlink -m $0`
CURDIR=`dirname ${CURFILE}`
source ${CURDIR}/utils.sh

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
