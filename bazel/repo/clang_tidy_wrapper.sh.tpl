#!/bin/bash

set -e

# HACK(chenhao94): Bazel will redefine built-in macros (e.g., "__DATE__") for
# deterministic builds, and it triggers the Clang Tidy check.
# See https://github.com/erenon/bazel_clang_tidy/issues/29
%{CLANG_TIDY_BIN} --checks=-clang-diagnostic-builtin-macro-redefined "$@"
