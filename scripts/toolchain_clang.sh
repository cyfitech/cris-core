#!/bin/bash

set -e

cd "$(dirname "$0")/.."

if [ -d 'run' ] && [ -w 'run' ] || [ -w '.' ]; then
    mkdir -p run

    cat << ________EOF | sed 's/^        //' > run/toolchain.bazelrc
        # Clang-specific Bazel Configurations

        build --compiler=clang

        build:rel   --config="lto"
        build:norel --config="nolto"

        build:lto --copt="-flto=thin"
        build:lto --linkopt="-flto=thin"

        build:nolto --copt="-fno-lto"
        build:nolto --linkopt="-fno-lto"
________EOF
fi
