#!/bin/bash

set -e

cd "$(dirname "$0")/.."

. scripts/toolchain.sh

! which ccache >/dev/null 2>&1 || ccache -z >&2

if [ "$#" -lt 1 ]; then
    bazel help
    exit 0
fi

BAZEL_CMD="$1"
shift

# - Need to set PATH instead of CC because Bazel does not like '/'s in the CC value.
# - Whitelist ccache dir in Bazel sandbox.
#   https://ccache.dev/manual/latest.html#config_cache_dir
PATH="$([ ! "$CC"  ] || dirname  "$CC" ):$([ ! "$CXX" ] || dirname "$CXX"):$PATH"   \
CC="$(  [ ! "$CC"  ] || basename "$CC" )"                                           \
CXX="$( [ ! "$CXX" ] || basename "$CXX")"                                           \
bazel "$BAZEL_CMD"                                                                  \
    --experimental_ui_max_stdouterr_bytes=-1                                        \
    --sandbox_writable_path="$(set -e;                                              \
        which ccache >/dev/null 2>&1                                                \
        && ccache --show-config 2>/dev/null                                         \
        | sed -n 's/^[[:space:]]*([^)]*)[[:space:]]*//p'                            \
        | sed 's/[[:space:]]*$//'                                                   \
        | sed -n 's/^cache_dir[[:space:]]*=[[:space:]]*//pi'                        \
        | head -n1                                                                  \
        | grep .                                                                    \
        || printf '/dev/null')"                                                     \
    --sandbox_writable_path="$(set -e;                                              \
        which ccache >/dev/null 2>&1                                                \
        && ccache --show-config 2>/dev/null                                         \
        | sed -n 's/^[[:space:]]*([^)]*)[[:space:]]*//p'                            \
        | sed 's/[[:space:]]*$//'                                                   \
        | sed -n 's/^temporary_dir[[:space:]]*=[[:space:]]*//pi'                    \
        | head -n1                                                                  \
        | grep .                                                                    \
        || printf '/dev/null')"                                                     \
    --sandbox_writable_path="$(set -e;                                              \
        which ccache >/dev/null 2>&1                                                \
        && [ "$HOME" ]                                                              \
        && [ -e "$HOME/.ccache" ]                                                   \
        && printf '%s/.ccache' "$HOME"                                              \
        || printf '/dev/null')"                                                     \
    --sandbox_writable_path="$(set -e;                                              \
        which ccache >/dev/null 2>&1                                                \
        && [ "$HOME" ]                                                              \
        && [ -e "$HOME/.cache/ccache" ]                                             \
        && printf '%s/.cache/ccache' "$HOME"                                        \
        || printf '/dev/null')"                                                     \
    --sandbox_writable_path="$(set -e;                                              \
        which ccache >/dev/null 2>&1                                                \
        && [ "$HOME" ]                                                              \
        && [ -e "$HOME/Library/Caches/ccache" ]                                     \
        && printf '%s/Library/Caches/ccache' "$HOME"                                \
        || printf '/dev/null')"                                                     \
    --sandbox_writable_path="$(set -e;                                              \
        which ccache >/dev/null 2>&1                                                \
        && [ "$XDG_CACHE_HOME" ]                                                    \
        && [ -e "$XDG_CACHE_HOME/ccache" ]                                          \
        && printf '%s/ccache' "$XDG_CACHE_HOME"                                     \
        || printf '/dev/null')"                                                     \
    "$@"

! which ccache >/dev/null 2>&1 || ccache -s >&2
