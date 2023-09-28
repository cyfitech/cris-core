#!/bin/bash

set -e

cd "$(dirname "$0")/.."

if [ -d 'run' ] && [ -w 'run' ] || [ -w '.' ]; then
    mkdir -p run

    cat << ________EOF | sed 's/^        //' > run/toolchain.bazelrc
        # GCC-specific Bazel Configurations

        build --linkopt="-fuse-ld=gold"
        build --nostart_end_lib

        build:rel   --config="lto"
        build:norel --config="nolto"

        build:lto --copt="-flto"
        build:lto --linkopt="-flto"

        build:nolto --copt="-fno-lto"
        build:nolto --linkopt="-fno-lto"
________EOF
fi
