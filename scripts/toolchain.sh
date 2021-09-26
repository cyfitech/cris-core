#!/bin/bash

set -e

pushd "$(dirname "$0")/.."

[ "$CC"  ] || export CC='clang-12'
[ "$CXX" ] || export CXX='clang++-12'

if which ccache >/dev/null 2>&1 && ([ -d 'run' ] && [ -w 'run' ] || [ -w '.' ]); then
    rm -rf 'run/toolchain'
    mkdir -p 'run/toolchain'
    ln -sf "$(which ccache)" "run/toolchain/$CC"
    ln -sf "$(which ccache)" "run/toolchain/$CXX"
    export CC="$( realpath -e "run/toolchain")/$CC"
    export CXX="$(realpath -e "run/toolchain")/$CXX"
fi

popd
