#!/bin/bash

CUR_DIR=$(realpath $(dirname ${BASH_SOURCE[0]}))
WORKSPACE_DIR=$(realpath ${CUR_DIR}/..)

OUTPUT_DIR=${WORKSPACE_DIR}/prebuilt_out

while [[ $# -gt 0 ]]; do
    option=$1
    shift
    case ${option} in
        -o|--output)
            OUTPUT_DIR=$(realpath "$1")
            shift
            ;;
        *)
            echo "Unknown options: ${option}"
            exit 1
            ;;
    esac
done

if [[ -e ${OUTPUT_DIR} ]]; then
    echo "${OUTPUT_DIR} exists."
    exit
fi

mkdir -p ${OUTPUT_DIR}

BIN_DIR=${WORKSPACE_DIR}/bazel-bin

if [[ ! -e ${BIN_DIR} ]]; then
    echo "No bazel output, maybe run 'bazel build' first"
    exit 1
fi

cp -rL ${BIN_DIR}/_virtual_includes/corelib ${OUTPUT_DIR}/includes

mkdir ${OUTPUT_DIR}/libs

cp ${BIN_DIR}/libcorelib.{a,so} ${OUTPUT_DIR}/libs
