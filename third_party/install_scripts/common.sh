#!/bin/bash
source ${install_script_dir}/../utils.sh


function decompress_cmd() {
    if [ `echo $1 | grep ".tar.gz$"` ]; then
        echo "tar -zxf"
    elif [ `echo $1 | grep ".tgz$"` ]; then
        echo "tar -zxf"
    elif [ `echo $1 | grep ".zip$"` ]; then
        echo "unzip"
    elif [ `echo $1 | grep ".tar.xz$"` ]; then
        echo "tar -Jxf"
    else
        echo ""
    fi
}

function get_url(){
    if [ -f ${pkgs_dir}/${1}.url ]; then
        local src="`cat ${pkgs_dir}/${1}.url | awk -F'\t' '{print $1}'`"
    else
        local src=`find ${pkgs_dir} -name ${1}-\* `
    fi
    echo $src
}

function decompress() {
    execshell "rm -rf $1"
    execshell "mkdir -p $1"
    execshell "pushd $1"
    if [ -f ${pkgs_dir}/${1}.url ]; then
        local pkg=`ls`
        check_return "pkg=`ls` failed"
        if [ "X$pkg" == "X" ]; then
            local src="`cat ${pkgs_dir}/${1}.url | awk -F'\t' '{print $1}'`"
            if [ "X${GITHOST}" == "X" ]; then
                GITHOST=github.com
            fi
            export GITHOST
            execshell "wget \"${src}\""
        fi
    else
        execshell "cp ${pkgs_dir}/${1}-* ."
    fi
    local pkg=`ls`
    check_return "pkg=`ls` failed"
    local cmd=`decompress_cmd $pkg`
    if [ "X$cmd" == "X" ]; then
        log_fatal "unkown compress format $pkg"
        exit 1
    fi
    execshell "$cmd $pkg"
    execshell "popd"
}

function install_pkg_common_preprocess() {
    export CUR_SCRIPT_TS="`stat -c %Y ${install_script_dir}/install_$1.sh`"
    if [ -f ${pkgs_dir}/${1}.url ]; then
        export CUR_PKG_TS="`stat -c %Y ${pkgs_dir}/${1}.url`"
    else
        export CUR_PKG_TS="`stat -c %Y ${pkgs_dir}/${1}-*`"
    fi
    CUR_PATCHES_TS=""
    if [ "${pkg_patch_list}" != "" ]; then
        for patch in ${pkg_patch_list}; do
            if [ "${CUR_PATCHES_TS}" = "" ]; then
                CUR_PATCHES_TS="`stat -c %Y ${patch}`"
            else
                CUR_PATCHES_TS="${CUR_PATCHES_TS},`stat -c %Y ${patch}`"
            fi
        done
    fi
    export CUR_PATCHES_TS
    if [ -f $1_INSTALLED_FLAG ]; then
        local old_pkg_ts=`awk -F"=" '{if($1=="CUR_PKG_TS"){print $2}}' $1_INSTALLED_FLAG`
        local old_script_ts=`awk -F"=" '{if($1=="CUR_SCRIPT_TS"){print $2}}' $1_INSTALLED_FLAG`
        local old_patches_ts=`awk -F"=" '{if($1=="CUR_PATCHES_TS"){print $2}}' $1_INSTALLED_FLAG`
        if [ "${old_pkg_ts}" = "${CUR_PKG_TS}" \
                -a "${old_script_ts}" = "${CUR_SCRIPT_TS}" \
                -a "${old_patches_ts}" = "${CUR_PATCHES_TS}" ]; then
            log_notice "`color 39 $1` already installed"
            return 1
        else
            log_notice "`color 39 $1` updated, need to be rebuild"
            rm -f $1_INSTALLED_FLAG
        fi
    fi

    execshell "decompress $1"
    execshell "pushd $1"
    local pkgdir=`find . -mindepth 1 -maxdepth 1 -type d -name "*"`
    check_return "find . -mindepth 1 -maxdepth 1 -type d -name '*' failed"
    if [ "X$pkgdir" = "X" ]; then
        log_fatal "cannnot find $1 related decompressed dir"
        exit 1
    fi
    execshell "pushd $pkgdir"
    execshell "pushd ./$subdir"
}

function install_pkg_common_postprocess() {
    execshell "popd"
    execshell "popd"
    execshell "popd"
    execshell "rm -rf $1"
    execshell "cat /dev/null > $1_INSTALLED_FLAG"
    execshell "echo -e \"CUR_PKG_TS=${CUR_PKG_TS}\" >> $1_INSTALLED_FLAG"
    execshell "echo -e \"CUR_SCRIPT_TS=${CUR_SCRIPT_TS}\" >> $1_INSTALLED_FLAG"
    execshell "echo -e \"CUR_PATCHES_TS=${CUR_PATCHES_TS}\" >> $1_INSTALLED_FLAG"
}


function install_pkg(){
    local pkg_install_func=$1
    shift;
    install_pkg_common_preprocess "${1}"
    if [ $? -eq 0 ]; then
        execshell ${pkg_install_func}
        install_pkg_common_postprocess "${1}"
    fi
    return 0
}

function inner_install_cmake_pkg(){
    [ "X$cmake_preprocess" != "X" ] && execshell "$cmake_preprocess"
    [[ -z "$build_type"  ]] && build_type=Release
    [[ ! -z "$cmake_list_dir"  ]] && execshell "pushd $cmake_list_dir"
    execshell "mkdir -p _build" && execshell "pushd _build"
    execshell "$CMAKE .. -DCMAKE_BUILD_TYPE=$build_type -DCMAKE_PREFIX_PATH=$prefix
    -DCMAKE_INSTALL_PREFIX=$prefix -DCMAKE_INSTALL_LIBDIR=lib $cmake_exdefine"
    execshell "make -j$J"
    [ "X$install_cmd" = "X" ] && execshell "make install" || execshell "$install_cmd"
    [ "X$cmake_postprocess" != "X" ] && execshell "$cmake_postprocess"
    execshell "popd"
    [[ ! -z "$cmake_list_dir"  ]] && execshell "popd"
    return 0
}

function install_cmake_pkg() {
    install_pkg inner_install_cmake_pkg $*
    return 0
}

function install_configure_pkg() {
    install_pkg_common_preprocess "$1"
    if [ $? -eq 0 ]; then
        execshell "$configure_preprocess"
        execshell "$configure_predef ./configure --prefix=$prefix $configure_flags"
        execshell "${make_predef} make -j$J $make_flags"
        [ "X$install_cmd" = "X" ] && execshell "make install" || execshell "$install_cmd"
        install_pkg_common_postprocess "$1"
    fi
    return 0
}

function inner_install_make_pkg() {
    execshell "$make_preprocess"
    execshell "make -j$J $make_flags"
    [ "X$install_cmd" = "X" ] && execshell "make install" || execshell "PREFIX=$prefix $install_cmd"
}

function install_make_pkg() {
    install_pkg inner_install_make_pkg $*
    return 0
}

