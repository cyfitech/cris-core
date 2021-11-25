#!/bin/bash

set -e

cd "$(dirname "$0")/.."

. scripts/codebase_utils.sh

bazel_common_all fetch "$@"
