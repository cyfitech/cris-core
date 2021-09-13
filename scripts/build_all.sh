#!/bin/bash

set -e

cd "$(dirname "$0")/.."

for cmd in bazel; do
    which "$cmd" >/dev/null
done

. scripts/toolchain.sh

! which ccache >/dev/null 2>&1 || ccache -z

bazel build "$@" '//...'

! which ccache >/dev/null 2>&1 || ccache -s
