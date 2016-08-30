#! /bin/bash

set -e

SCRIPT_DIR=$(cd $(dirname $0); pwd)
SRC_DIR=$(cd ${SCRIPT_DIR}/../..; pwd)
BUILD_DIR=/mnt/build/
OUT_DIR=/mnt/out/
CCACHE_DIR=/mnt/ccache

export PATH=/usr/lib/ccache/:${PATH}
export CCACHE_DIR

cd ${BUILD_DIR}

cmake -DCMAKE_BUILD_TYPE=Release ${SRC_DIR}

make -j $(nproc)

cp netup_gpu_meter ${OUT_DIR}/
