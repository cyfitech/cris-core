#!/bin/bash

set -e

cd "$(dirname "$0")/.."

gcc_version=$(set -e
    "$CC" -v 2>&1 \
    | sed -n 's/.*gcc[[:space:]][[:space:]]*version[[:space:]][[:space:]]*\([1-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/p' \
    | sort -rV \
    | head -n1 \
    | grep . \
    || echo "unknown"
)

if [ -d 'run' ] && [ -w 'run' ] || [ -w '.' ]; then
    mkdir -p run

    cat << ________EOF | sed 's/^        //' > run/toolchain.bazelrc
        # GCC-specific Bazel Configurations

        build --linkopt="-fuse-ld=gold"
        build --nostart_end_lib

        build:rel   --config="lto"
        build:norel --config="nolto"

        build:lto --copt="-flto=auto"
        build:lto --linkopt="-flto=auto"

        build:nolto --copt="-fno-lto"
        build:nolto --linkopt="-fno-lto"
________EOF
fi

# GCC 12 and 13 are firing false-positive stringop-overflow warnings.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=106297
if printf '%s' "$gcc_version" | grep -q -e'^'{12,13}'\.'; then
    cat << ________EOF | sed 's/^        //' >> run/toolchain.bazelrc

        # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=106297
        build --copt=-Wno-stringop-overflow
        build --linkopt=-Wno-stringop-overflow
________EOF
fi

# GCC 12 avx512 math function raises uninitialized variable warning
# 12.1 and 12.2 are affected while 12.3 is OK.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105593
if printf '%s' "$gcc_version" | grep -q -e'^12\.'{1,2}'\.'; then
    cat << ________EOF | sed 's/^        //' >> run/toolchain.bazelrc

        # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105593
        build --copt=-Wno-maybe-uninitialized
        build --linkopt=-Wno-maybe-uninitialized
________EOF
fi

# GCC 12 (at least up to 12.3) and above may have false alarm on dead code of libstdc++.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105329
if [ ! "Enable when necessary." ] && printf '%s' "$gcc_version" | grep -q -e'^'{{12,13}'\.'{0,1,2,3},'14\.'{0,1}}'\.'; then
    cat << ________EOF | sed 's/^        //' >> run/toolchain.bazelrc

        # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105329
        build --copt=-Wno-restrict
________EOF
fi
