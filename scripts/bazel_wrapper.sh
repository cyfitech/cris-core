#!/bin/bash

set -e

cd "$(dirname "$0")/.."

. scripts/toolchain.sh

! which ccache >/dev/null 2>&1 || ccache -z

# Need to set PATH instead of CC because Bazel does not like '/'s in the CC value.
PATH="$([ ! "$CC"  ] || dirname  "$CC" ):$([ ! "$CXX" ] || dirname "$CXX"):$PATH"   \
CC="$(  [ ! "$CC"  ] || basename "$CC" )"                                           \
CXX="$( [ ! "$CXX" ] || basename "$CXX")"                                           \
bazel "$@"

! which ccache >/dev/null 2>&1 || ccache -s
