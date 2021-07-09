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

function publish() {
    if [ 0"${REGISTRY}" == "0" ]; then
        echo "REGISTRY not set"
        return 1
    fi
    git_tag = `git describe --exact-match HEAD || git rev-parse HEAD`
    if [ 0"${VERSION}" == "0" ]; then
        VERSION=${git_tag}
    fi
    echo "tag=${tag}"
    echo "REGISTRY=${REGISTRY}"
    echo "VERSION=${VERSION}"
    if [ 0"${VERSION}" != 0"${tag}" ] && [ 0"${VERSION}" != 0"dev-${tag}" ]; then
        echo -e "VERSION not match with tag"
    fi
    IMAGE=${REGISTRY}/prpc:${VERSION}
    echo "IMAGE=${IMAGE}"
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
        echo -e "please commit your change before publish"
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
        echo -e "unkown cmd"
        return 1
    ;;
esac
