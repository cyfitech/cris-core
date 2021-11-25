#!/bin/bash

set -e

cd "$(dirname "$0")/.."

for cmd in git rsync; do
    which "$cmd" >/dev/null 2>&1
done

tmpdir="$(mktemp -d)"

for i in $(paste -sd"$(printf '\v')" WORKSPACE | sed 's/git_repository[[:space:]]*([^)]*name[[:space:]]*=[[:space:]]*"\([^\"]*\)"[^)]*remote[[:space:]]*=[[:space:]]*"\([^"]*\)"/\n########\1,\2\n/g' | sed -n 's/^########\(cris.*\)/\1/p'); do
    repo="$(cut -d, -f1 <<< "$i")"
    # Use SSH for write because GitHub does not accept HTTPS auth without PAT.
    url="$( cut -d, -f2 <<< "$i," | sed 's/^https:\/\/github\.com\//git@github\.com:/')"
    git clone --single-branch "$url" "$tmpdir/$repo"
    git -C "$tmpdir/$repo" checkout -b sync_infra
    rsync -avPcR .bazelrc .clang-format ./.github/workflows/build.yml .gitignore Makefile $(find "./scripts" -mindepth 1 -maxdepth 1 | grep -v '\/codebase_utils\.sh') "$tmpdir/$repo/"
    git -C "$tmpdir/$repo" add -A .
    ! git -C "$tmpdir/$repo" --no-pager diff --exit-code master || continue
    make -C "$tmpdir/$repo" lint
    git -C "$tmpdir/$repo" --no-pager diff
    git -C "$tmpdir/$repo" commit -am "[Automated] Sync infrastructure configs/scripts from upstream."
    git -C "$tmpdir/$repo" push -fu origin sync_infra
done

rm -rf "$tmpdir"
