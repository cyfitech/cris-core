#!/bin/bash

set -e

if [ ! "$CC" ] && [ ! "$CXX" ]; then
    for ver in -{2,1}{9,8,7,6,5,4,3,2,1,0} ''; do
        which "clang$ver"   >/dev/null 2>&1 || continue
        which "clang++$ver" >/dev/null 2>&1 || continue
        if ! "clang$ver" --version                                                          \
            | sed -n 's/.*clang version \([1-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/p'   \
            | head -n1                                                                      \
            | cut -d. -f1                                                                   \
            | xargs -rI{} expr {} - 12                                                      \
            | grep '^[0-9]' >/dev/null
        then
            printf '\033[33m[WARNING] Found compiler "%s" with insufficient version.\033[0m' "clang$ver" >&2
            continue
        fi
        export CC="clang$ver" CXX="clang++$ver"
        break
    done
fi

if [ ! "$CC" ] || [ ! "$CXX" ]; then
    . scripts/detect_distro.sh
    printf '\033[31m[ERROR] Failed to detect toolchain on distro %s.\033[0m\n' "$DISTRO_ID-$DISTRO_VERSION_ID" >&2
    if [ "$CC" ] || [ "$CXX" ]; then
        printf '\033[33m[WARNING] Provide a consistent pair of $CC and $CXX, or leave them empty for auto detection.\033[0m\n' >&2
    fi
    exit 1
fi
