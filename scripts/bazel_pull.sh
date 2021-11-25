#!/bin/bash

set -e

cd "$(dirname "$0")/.."

# Pre-check commands used by this script.
for cmd in base64 diff git grep sed xargs; do
    which "$cmd" >/dev/null
done

# Vertical tab is reserved to be used as an inline LF.
if grep "$(printf '\v')" 'WORKSPACE' >/dev/null; then
    printf '\033[31m[ERROR] Vertical tab is reserved to be used during scanning. Please use whitespace instead for the "____"-noted regions:\033[0m\n' >&2
    grep "$(printf '\v')" 'WORKSPACE' | sed 's/'"$(printf '\v')"'/____/g' | sed 's/^/\[ERROR\]     /' | xargs -0 printf "\033[31m%s\033[0m\n" >&2
    exit 1
fi

# Leading pattern for extraction-sed.
sed_magic="$(seq 20 | base64)"

# Note(Tongliang Liao):
#     Extraction-sed:
#         - Throw away comments.
#         - Replace LF with whatever space to put entire file into one line.
#         - Find RoI (function with given name and args) by sed.
#         - Surround detected RoI with LF to put in its own line, and prepend a magic pattern.
#         - Filter out lines starting with the magic.
#     Replacement-sed:
#         - Replace LF with a dedicated space char (e.g. vertical tab) to put entire file into one line.
#         - Find RoI and replace its args.
#         - Replace that dedicated space back to LF.
#     For unordered args, use multiple patterns to cover all orders, or multiple single-arg passes together with some sorting.

# Extraction-sed for gathering git repo labels into a list.
repos=''
repos="$repos $(sed 's/#.*//' 'WORKSPACE'   \
    | paste -sd"$(printf '\v')" -           \
    | sed 's/git_repository[[:space:]]*([^)]*name[[:space:]]*=[[:space:]]*"\([^"]*\)"[^)]*branch[[:space:]]*=[[:space:]]*"[^"]*"[^)]*)/\n'"$sed_magic"'\1\n/g'  \
    | sed 's/git_repository[[:space:]]*([^)]*branch[[:space:]]*=[[:space:]]*"[^"]*"[^)]*name[[:space:]]*=[[:space:]]*"\([^"]*\)"[^)]*)/\n'"$sed_magic"'\1\n/g'  \
    | sed -n 's/^'"$sed_magic"'//p')"
repos="$(xargs -r <<< "$repos")"

[ ! "$repos" ] || printf '\033[36m[INFO] Found candidate repo(s): %s\033[0m\n' "$repos" >&2

# Extraction-sed for appending remote/branch args, as a comma-separated tuple, for the given repo list.
repos_rb=''
for repo in $repos; do
    # Escape special characters so that they can be used as literal in sed regex.
    repo_esc="$(sed 's/\([\\\/\.\-]\)/\\\1/g' <<< "$repo")"

    remote="$(sed 's/#.*//' 'WORKSPACE' \
        | paste -sd"$(printf '\v')" -   \
        | sed 's/git_repository[[:space:]]*([^)]*name[[:space:]]*=[[:space:]]*"'"$repo_esc"'"[^)]*remote[[:space:]]*=[[:space:]]*"\([^"]*\)"[^)]*)/\n'"$sed_magic"'\1\n/g'  \
        | sed 's/git_repository[[:space:]]*([^)]*remote[[:space:]]*=[[:space:]]*"\([^"]*\)"[^)]*name[[:space:]]*=[[:space:]]*"'"$repo_esc"'"[^)]*)/\n'"$sed_magic"'\1\n/g'  \
        | sed -n 's/^'"$sed_magic"'//p' \
        | grep '[^[:space:]]'           \
        | head -n1)"
    if [ ! "$remote" ]; then
        printf '\033[33m[WARNING] Remote URL for "%s" not found. Skipped.\033[0m\n' "$repo" >&2
        continue
    fi

    branch="$(sed 's/#.*//' 'WORKSPACE' \
        | paste -sd"$(printf '\v')" -   \
        | sed 's/git_repository[[:space:]]*([^)]*name[[:space:]]*=[[:space:]]*"'"$repo_esc"'"[^)]*branch[[:space:]]*=[[:space:]]*"\([^"]*\)"[^)]*)/\n'"$sed_magic"'\1\n/g'  \
        | sed 's/git_repository[[:space:]]*([^)]*branch[[:space:]]*=[[:space:]]*"\([^"]*\)"[^)]*name[[:space:]]*=[[:space:]]*"'"$repo_esc"'"[^)]*)/\n'"$sed_magic"'\1\n/g'  \
        | sed -n 's/^'"$sed_magic"'//p' \
        | grep '[^[:space:]]'           \
        | head -n1)"
    if [ ! "$branch" ]; then
        printf '\033[33m[WARNING] Branch for "%s" not found. Skipped.\033[0m\n' "$repo" >&2
        continue
    fi

    repos_rb="$repos_rb $repo,$remote,$branch"
done

# Query remote for latest commit hash on the branch, given the "repo,remote,branch" tuple list.
# Return the list with one more column "commit".
# Run this step in parallel if possible because it is relatively slow and need a few retries due to network access.
repos_rbc="$(xargs -rn1 <<< "$repos_rb" | $(which parallel >/dev/null 2>&1 && echo 'parallel -j0' || echo 'xargs -rn1') 'bash -c '"'"'
    set -e +x
    repo="$(  cut -d, -f1 <<< "{}"  )"
    remote="$(cut -d, -f2 <<< "{}," )"
    branch="$(cut -d, -f3 <<< "{},,")"
    for attempt in $(seq 3 -1 0); do
        commit="$(git --no-pager ls-remote --exit-code -q "$remote" "$branch" | head -n1 | cut -f1)"
        [ ! "$commit" ] || break
    done
    if [ ! "$commit" ]; then
        printf "\\033[33m[WARNING] Commit hash for branch \"%s\" in repo \"%s\" not found. Skipped.\\033[0m\\n" "$branch" "$repo" >&2
        exit 0
    fi
    echo "$repo,$remote,$branch,$commit"
'"'")"
if [ "$(xargs -n1 <<< "$repos_rb" | wc -l)" -ne "$(xargs -n1 <<< "$repos_rbc" | wc -l)" ]; then
    printf '\033[31m[ERROR] Not all repos are correctly retrieved.\033[0m\n' >&2
    exit 1
fi

tmp_ws_dir=''
for tuple in $repos_rbc; do
    [ "$tmp_ws_dir" ] || tmp_ws_dir="$(mktemp -d)"
    (
        set -e

        # Decode the input tuple list and generate their sed-escaped patterns.
        repo="$(  cut -d, -f1 <<< "$tuple"   )"
        remote="$(cut -d, -f2 <<< "$tuple,"  )"
        branch="$(cut -d, -f3 <<< "$tuple,," )"
        commit="$(cut -d, -f4 <<< "$tuple,,,")"
        repo_esc="$(  sed 's/\([\\\/\.\-]\)/\\\1/g' <<< "$repo"  )"
        commit_esc="$(sed 's/\([\\\/\.\-]\)/\\\1/g' <<< "$commit")"

        # CoW for tmp file.
        [ -e "$tmp_ws_dir/WORKSPACE" ] || cp -f {,"$tmp_ws_dir"/}'WORKSPACE'

        # Replacement-sed to turn arg "branch" into "commit".
        paste -sd"$(printf '\v')" "$tmp_ws_dir/WORKSPACE"    \
        | sed 's/\(git_repository[[:space:]]*([^)]*name[[:space:]]*=[[:space:]]*"'"$repo_esc"'"[^)]*\)branch\([[:space:]]*=[[:space:]]*"\)[^"]*\("[^)]*)\)/\1commit\2'"$commit_esc"'\3/g'   \
        | sed 's/\(git_repository[[:space:]]*([^)]*\)branch\([[:space:]]*=[[:space:]]*"\)[^"]*\("[^)]*name[[:space:]]*=[[:space:]]*"'"$repo_esc"'"[^)]*)\)/\1commit\2'"$commit_esc"'\3/g'   \
        | tr '\v' '\n'                      \
        > "$tmp_ws_dir/WORKSPACE.expanding"

        # POSIX transactional commit for updates.
        mv -f "$tmp_ws_dir/WORKSPACE"{'.expanding',}
        printf '\033[36m[INFO] Expanded branch "%s" -> "%s" for repo "%s" (%s).\033[0m\n' "$branch" "$commit" "$repo" "$remote" >&2
    )
done
if [ "$tmp_ws_dir" ]; then
    # Only write back if necessary.
    if [ -e "$tmp_ws_dir/WORKSPACE" ] && ! diff {"$tmp_ws_dir/",}'WORKSPACE' >/dev/null; then
        if which git >/dev/null 2>&1 && ! git --no-pager diff --exit-code 'WORKSPACE'; then
            printf '\033[33m[WARNING] WORKSPACE file is dirty in git. Here is the version before change:\033[0m\n' >&2
            sed 's/^/\[WARNING\]     /' 'WORKSPACE' | sed "s/$(printf '\r')//g" | tr '\n' '\0' | xargs -0 printf '\033[33m%s\033[0m\n' >&2
        fi
        cat "$tmp_ws_dir/WORKSPACE" > 'WORKSPACE'
        printf '\033[32m[INFO] WORKSPACE updated.\033[0m\n' >&2
    else
        printf '\033[32m[INFO] No update for candidate repo(s).\033[0m\n' >&2
    fi
    rm -rf "$tmp_ws_dir"
else
    printf '\033[32m[INFO] Nothing to update.\033[0m\n' >&2
fi
