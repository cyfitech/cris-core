#!/bin/bash

set -e

cd "$(dirname "$0")/.."

if [ -d 'run' ] && [ -w 'run' ] || [ -w '.' ]; then
    mkdir -p run

    cat << ________EOF | sed 's/^        //' > run/toolchain.bazelrc
        # GCC-specific Bazel Configurations

        build:rel --config="lto"
        build:lto --copt="-flto"
        build:lto --copt="-fuse-linker-plugin"
        build:lto --linkopt="-flto"
        build:lto --linkopt="-fuse-linker-plugin"
________EOF
fi
