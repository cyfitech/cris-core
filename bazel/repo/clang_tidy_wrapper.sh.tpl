#!/bin/bash

set -e

# HACK(chenhao94): Skip all the "textual" sources.
if printf "%s\n" "$@" | grep "\.txt\.h\(pp\)\?$" > /dev/null; then
    exit 0;
fi

# HACK(chenhao94): Bazel will redefine built-in macros (e.g., "__DATE__") for
# deterministic builds, and it triggers the Clang Tidy check.
# See https://github.com/erenon/bazel_clang_tidy/issues/29
# HACK(chenhao94): Skip all the generated protobuf headers.
%{CLANG_TIDY} \
    --checks=-clang-diagnostic-builtin-macro-redefined \
    --line-filter='[
            {"name":".pb.h","lines":[[9999999,9999999]]},
            {"name":".h"},
            {"name":".hpp"},
            {"name":".c"},
            {"name":".cc"},
            {"name":".cpp"}
        ]' \
    "$@"
