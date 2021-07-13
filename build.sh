#!/bin/bash
set -e
PROJECT_ROOT=`pwd`
echo ${PROJECT_ROOT}

function publish() {
    if [ "${VERSION}" == "" ]; then
        VERSION=0.0.0
        tag=latest
    else
        tag=`git describe --exact-match HEAD`
    fi
    echo "VERSION=${VERSION}"
    echo "tag=${tag}"
    if [ "${VERSION}" != "${tag}" ]; then
        echo -e "VERSION not match with tag"
    fi

    image=4pdosc/prpc:${tag}
    docker build -t ${image} -f docker/Dockerfile .
    if [ "${VERSION}" != "0.0.0" ]; then
        docker push ${image}
    fi
}

function publish_check() {
    git diff
    local git_diff=`git diff` 
    if [ "$git_diff" == "" ]; then
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
