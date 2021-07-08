#!/bin/bash
set -e
PROJECT_ROOT=`pwd`
echo ${PROJECT_ROOT}

function build() {
    mkdir -p ${PROJECT_ROOT}/build
    pushd ${PROJECT_ROOT}/build
    cmake ../
    make -j$J
    popd
}

function current_publish_version() {
    local folder="$(pwd)"
    [ -n "$1" ] && folder="$1"
    git -C "$folder" rev-parse --abbrev-ref HEAD | grep -v HEAD || \
    git -C "$folder" describe --exact-match HEAD || \
    git -C "$folder" rev-parse HEAD
}

function publish() {
    if [ 0"${REGISTRY}" == "0" ]; then
        echo "REGISTRY not set"
        return 1
    fi
    if [ 0"${VERSION}" == "0" ]; then
        echo "VERSION not set"
        return 1
    fi
    IMAGE=${REGISTRY}/prpc:${VERSION}
    docker build -t ${IMAGE} .
    docker tag ${IMAGE} ${REGISTRY}/${IMAGE}
    docker push ${REGISTRY}/${IMAGE}
}

function publish_check() {
    git diff
    local git_diff=`git diff` 
    if [ 0"$git_diff" == "0" ]; then
        echo "git nodiff"
    else
        echo -e "Please commit your change before publish"
        return 1
    fi
}

function unit_test() {
    pushd ${PROJECT_ROOT}/build
    make test
    popd
}

case "$1" in
    test)
        unit_test
    ;;
    publish)
        publish_check
        publish
    ;;
    build|"")
        build
    ;;
    *)
        echo "unkown cmd"
        return 1
    ;;
esac
