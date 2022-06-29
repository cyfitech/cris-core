#!/bin/bash

set -e

cd "$(dirname "$0")/.."

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

for cmd in "$CLANG_FORMAT" find grep python3 sed xargs; do
    which "$cmd" >/dev/null
done

# CRLF -> LF.
# This should not be used on Windows.
! which dos2unix >/dev/null 2>&1                                                                        \
|| ls -1A                                                                                               \
| grep -v '^.git$'                                                                                      \
| sed 's/\r//g'                                                                                         \
| tr '\n' '\0'                                                                                          \
| $(which parallel >/dev/null 2>&1 && echo "parallel -j$(nproc) -m" || echo 'xargs -rI{}') -0           \
    find {} -type f                                                                                     \
| sed 's/\r//g'                                                                                         \
| tr '\n' '\0'                                                                                          \
| $(which parallel >/dev/null 2>&1 && echo "parallel -j$(nproc) -m" || echo 'xargs -r'   ) -0           \
    grep -HI "$(printf '\r$')"                                                                          \
| cut -d: -f1                                                                                           \
| grep -v $(which git >/dev/null                                                                        \
    && git submodule foreach -q pwd                                                                     \
    | xargs -rI{} realpath --relative-to="$(pwd)" {}                                                    \
    | sed 's/\/*$/\//'                                                                                  \
    | sed 's/\([\\\/\.\-]\)/\\\1/g'                                                                     \
    | sed 's/^/\-e\^/'                                                                                  \
    || echo '^$')                                                                                       \
| sort -u                                                                                               \
| sed 's/\r//g'                                                                                         \
| tr '\n' '\0'                                                                                          \
| $(which parallel >/dev/null 2>&1 && echo "parallel -j$(nproc) -m" || echo 'xargs -r'   ) -0 dos2unix -b -v

# If Python formatter is not installed, install it
python3 -c "import black" &> /dev/null || python3 -m pip install -U black
find . $(find . -maxdepth 1 -name .gitmodules -type f                           \
         | xargs -r sed -n 's/^[[:space:]]*path[[:space:]]*=[[:space:]]*//p'    \
         | grep -v '[[:space:]]'                                                \
         | cat - <(printf '%s\n' .git build out run tmp)                        \
         | xargs -r printf '-path ./%s -prune -o ')                             \
    -type f                                                                     \
| sort -u                                                                       \
| tr '\n' '\0'                                                                  \
| $(which parallel >/dev/null && echo "parallel -j$(nproc) -k -q" || echo 'xargs -rI{}') -0 bash -c 'set -e;
    FILE_NAME="{}"
    case "$FILE_NAME" in
        *.c | *.cc | *.cpp | *.h | *.hpp | *.proto)
            '"'$CLANG_FORMAT'"' -i "$FILE_NAME"
            ;;
        *.py)
            python3 -m black -q "$FILE_NAME"
            ;;
    esac
'
