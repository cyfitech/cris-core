#!/bin/bash

set -e

cd "$(dirname "$0")/.."

if [ -d 'run' ] && [ -w 'run' ] || [ -w '.' ]; then
    mkdir -p run

    cat << ________EOF | sed 's/^        //' > run/toolchain.bazelrc
        # GCC-specific Bazel Configurations

        build:rel --config="lto"
        build:lto --copt="-fwhole-program"
        build:lto --linkopt="-fwhole-program"
________EOF
fi
