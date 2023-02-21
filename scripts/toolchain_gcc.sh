#!/bin/bash

set -e

cd "$(dirname "$0")/.."

if [ -d 'run' ] && [ -w 'run' ] || [ -w '.' ]; then
    mkdir -p run

    cat << ________EOF | sed 's/^        //' > run/toolchain.bazelrc
        # GCC-specific Bazel Configurations

        build --linkopt="-fuse-ld=gold"

        build:rel --config="lto"
        build:lto --nostart_end_lib
        build:lto --copt="-flto"
        build:lto --linkopt="-flto"
________EOF
fi
