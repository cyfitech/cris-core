#!/bin/bash

set -e

cd "$(dirname "$0")/.."

. scripts/toolchain.sh

! which ccache >/dev/null 2>&1 || ccache -z

if [ "$#" -lt 1 ]; then
    bazel help
    exit 0
fi

BAZEL_CMD="$1"
shift

# Need to set PATH instead of CC because Bazel does not like '/'s in the CC value.
PATH="$([ ! "$CC"  ] || dirname  "$CC" ):$([ ! "$CXX" ] || dirname "$CXX"):$PATH"   \
CC="$(  [ ! "$CC"  ] || basename "$CC" )"                                           \
CXX="$( [ ! "$CXX" ] || basename "$CXX")"                                           \
bazel "$BAZEL_CMD"                                                                  \
      "$(! which ccache >/dev/null 2>&1 || ccache -sv 2>/dev/null                   \
         | sed -n 's/^[[:space:]]*cache directory[[:space:]:][[:space:]]*//pi'      \
         | grep .                                                                   \
         | head -n1                                                                 \
         | xargs -I{} printf '%s=%s' '--sandbox_writable_path' "{}" )"              \
      "$@"

! which ccache >/dev/null 2>&1 || ccache -s
