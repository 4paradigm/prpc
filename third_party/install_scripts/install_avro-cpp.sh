#!/bin/bash
source ${install_script_dir}/common.sh


#function install_avro() {
#    name="avro-cpp"
#    install_pkg_common_preprocess "$name"
#    if [ $? -eq 0 ]; then
#        #patch -f -p3 < ${prefix}/pkgs/avro.git*
#        patch -f -p1 < ${prefix}/pkgs/avro.pico.diff
#        execshell "mkdir _build" && execshell "pushd _build"
#        execshell "$CMAKE .. -DCMAKE_INSTALL_PREFIX=$prefix $cmake_exdefine"
#        execshell "make -j$J"
#        [ "X$install_cmd" = "X" ] && execshell "make install" || execshell "$install_cmd"
#        execshell "popd"
#        install_pkg_common_postprocess "$name"
#    fi
#    return 0
#}

pkg_patch_list="${patches_dir}/patch-avro.patch" \
    subdir=lang/c++ \
    cmake_preprocess="patch -f -p1 < ${patches_dir}/patch-avro.patch" \
    cmake_exdefine="-DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_C_FLAGS=-fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON" \
    execshell "install_cmake_pkg avro-cpp"

