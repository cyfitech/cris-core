#!/bin/bash

set -e

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
