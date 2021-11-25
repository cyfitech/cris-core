#!/bin/bash

set -e

cd "$(dirname "$0")/.."

. scripts/codebase_utils.sh
. scripts/toolchain.sh

! which ccache >/dev/null 2>&1 || ccache -z

bazel_common_all build "$@"

! which ccache >/dev/null 2>&1 || ccache -s
