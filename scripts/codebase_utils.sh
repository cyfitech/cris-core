#!/bin/bash

set -e

for cmd in bazel; do
    which "$cmd" >/dev/null
done

function bazel_common_all () {
    bazel "$@" '//...'
}
