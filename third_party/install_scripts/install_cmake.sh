#!/bin/bash
source ${install_script_dir}/common.sh

configure_flags="--parallel=$J" \
execshell "install_configure_pkg cmake"

