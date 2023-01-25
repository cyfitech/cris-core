#!/bin/bash

set -e

cd "$(dirname "$0")/.." >/dev/null

cc_detect_sample_code_dir="$(mktemp -d)"
pushd "$cc_detect_sample_code_dir" >/dev/null
trap "trap - SIGTERM; $(sed 's/^\(..*\)$/rm \-rf "\1"/' <<< "$cc_detect_sample_code_dir"); kill -- -'$$'" SIGINT SIGTERM EXIT

cat << EOF > cc_type.c
#include <stdio.h>
int main() {
    printf("%s\n",
#if defined(__clang__)
    "clang"
#elif defined(__GNUC__)
    "gcc"
#elif defined(_MSC_VER)
    "msvc"
#else
    "unknown"
#endif
    );
    return 0;
}
EOF

"$CC" cc_type.c -o cc_type
export CC_TYPE="$(./cc_type)"

popd > /dev/null
rm -rf "$cc_detect_sample_code_dir"
trap - SIGTERM SIGINT EXIT
