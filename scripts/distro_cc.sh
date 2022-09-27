#!/bin/bash

set -e

if [ ! "$CC" ] && [ ! "$CXX" ]; then
    for ver in -1{5,4,3,2,1} ''; do
        which "clang$ver"   >/dev/null 2>&1 || continue
        which "clang++$ver" >/dev/null 2>&1 || continue
        if ! "clang$ver" --version                                                          \
            | sed -n 's/.*clang version \([1-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/p'   \
            | head -n1                                                                      \
            | cut -d. -f1                                                                   \
            | xargs -rI{} expr {} - 11                                                      \
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
    export DISTRO_ID='<UNKNOWN_DISTRO>'
    export DISTRO_VERSION_ID='<UNKNOWN_VER>'
    case "$(uname -s)" in
    'Darwin')
        export DISTRO_ID='Darwin'
        if which sw_vers >/dev/null; then
            export DISTRO_ID="$(sw_vers -productName)"
            export DISTRO_VERSION_ID="$(sw_vers -productVersion)"
        fi
        ;;
    'Linux')
        [ ! -e '/etc/os-release' ] || . <(sed 's/^\(..*\)/export DISTRO_\1/' '/etc/os-release')
        ;;
    esac
    printf '\033[31m[ERROR] Failed to detect toolchain on distro %s.\033[0m\n' "$DISTRO_ID-$DISTRO_VERSION_ID" >&2
    if [ "$CC" ] || [ "$CXX" ]; then
        printf '\033[33m[WARNING] Provide a consistent pair of $CC and $CXX, or leave them empty for auto detection.\033[0m\n' >&2
    fi
    exit 1
fi
