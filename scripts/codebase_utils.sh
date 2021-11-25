#!/bin/bash

set -e

for cmd in bazel; do
    which "$cmd" >/dev/null
done

export CRIS_CODE_DIRS="$(sed 's/^[[:space:]]*//' <<< "
    includes
    src
    tests
" | grep '[^[:space:]]')"

function bazel_common_all () {
    bazel "$@" '//...'
}
