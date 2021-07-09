#!/bin/bash
set -e
PROJECT_ROOT=`pwd`
echo ${PROJECT_ROOT}

function publish() {
    if [ 0"${REGISTRY}" == "0" ]; then
        echo "REGISTRY not set"
        return 1
    fi
    tag=`git describe --exact-match HEAD || echo latest`
    if [ 0"${VERSION}" == "0" ]; then
        VERSION=${tag}
    fi
    echo "REGISTRY=${REGISTRY}"
    echo "VERSION=${VERSION}"
    echo "tag=${tag}"
    if [ 0"${VERSION}" != 0"${tag}" ]; then
        echo -e "VERSION not match with tag"
    fi
    if [ 0"${tag}" == "0latest" ]; then
        VERSION=0.0.0
    fi
    image=prpc:${tag}
    docker build -t ${image} .
    docker tag ${image} ${REGISTRY}/${image}
    docker push ${REGISTRY}/${image}
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

case "$1" in
    publish)
        publish_check
        publish
    ;;
    *)
        echo -e "unkown cmd"
        return 1
    ;;
esac
