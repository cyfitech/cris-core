#!/bin/bash

set -e

for cmd in docker sed; do
    which "$cmd" >/dev/null
done

src='ghcr.io/cyfitech/cris-build:ci'

sudo_docker="$([ -w '/var/run/docker.sock' ] || echo sudo) docker"

$sudo_docker pull "$src"
date="$($sudo_docker inspect -f '{{.Created}}' "$src" | sed -n 's/\([0-9][0-9]*\)\-\([01][0-9]\)\-\([0-3][0-9]\)T.*/\1\2\3/p')"
hash="$($sudo_docker inspect -f '{{.Id}}'      "$src" | cut -d: -f2 | sed 's/^\(............\).*/\1/')"

[ "$date" ]
[ "$hash" ]

for dst in {"$(cut -d: -f1 <<< "$src:")",'cajunhotpot/cris-build'}":$date"{,"-$hash"}; do
    $sudo_docker tag "$src" "$dst"
    $sudo_docker push "$dst"
done
