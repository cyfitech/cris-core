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
    # Need to set PATH instead of CC because Bazel does not like '/'s in the CC value.
    CC="$(basename "$CC")" PATH="$CR_CCACHE_CC_DIR:$PATH" bazel "$@" '//...'
}
