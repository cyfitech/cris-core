#!/bin/bash

set -e

cd "$(dirname "$0")/.."

for cmd in bazel; do
    which "$cmd" >/dev/null
done

bazel fetch "$@" '//...'
