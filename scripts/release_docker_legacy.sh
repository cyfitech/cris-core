#!/bin/bash

set -e

for cmd in docker sed; do
    ! which "$cmd" >/dev/null 2>&1 || continue
    printf '\033[31m[ERROR] Missing command "%s".\033[0m\n' "$cmd" >&2
    exit 1
done

sudo_docker="$([ -w '/var/run/docker.sock' ] || echo sudo) docker"

for dist in ubuntu2{0,2}04 debian11; do
    repo_name="cris-build-$dist"
    src="ghcr.io/cyfitech/$repo_name:ci"

    $sudo_docker pull "$src"
    date="$($sudo_docker inspect -f '{{.Created}}' "$src" | sed -n 's/\([0-9][0-9]*\)\-\([01][0-9]\)\-\([0-3][0-9]\)T.*/\1\2\3/p')"
    hash="$($sudo_docker inspect -f '{{.Id}}'      "$src" | cut -d: -f2 | sed 's/^\(............\).*/\1/')"
    [ "$date" ]
    [ "$hash" ]

    for dst in {"$(cut -d: -f1 <<< "$src:")","cajunhotpot/$repo_name"}":$date"{,"-$hash"}; do
        $sudo_docker tag "$src" "$dst"
        $sudo_docker push "$dst"
    done
done
