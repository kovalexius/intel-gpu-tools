#! /bin/bash

TARGET=intel_gpu_tools
SCRIPT_DIR=$(cd $(dirname ${0}); pwd)
SRC_DIR=${SCRIPT_DIR}/..
REVISION=$(cd ${SRC_DIR}; git rev-parse HEAD)
if [ "${JENKINS_HOME}" ]; then
    mkdir -p jenkins
    cd jenkins
fi
OUT_DIR=$(pwd)/out/${TARGET}
BUILD_DIR=$(pwd)/build/${TARGET}
CCACHE_DIR=$(pwd)/ccache/${TARGET}
IMAGE_TAG=$(head -n 1 ${SCRIPT_DIR}/build.tag)
IMAGE_NAME=intel_gpu  #### ATTENTION! Set url to right Docker Image!!!!

mkdir -p ${BUILD_DIR} ${OUT_DIR} ${CCACHE_DIR}

docker run --rm \
    --volume ${SRC_DIR}:/mnt/src \
    --volume ${BUILD_DIR}:/mnt/build \
    --volume ${OUT_DIR}:/mnt/out \
    --volume ${CCACHE_DIR}:/mnt/ccache \
    --user root \
    -it \
    ${IMAGE_NAME} \
    /bin/bash

