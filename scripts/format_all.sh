#!/bin/bash

set -e

cd "$(dirname "$0")/.."

# Clang Format Auto Detection
CLANG_FORMAT_DEFAULT_VERSION=12
export CLANG_FORMAT="${CLANG_FORMAT:-$(set -e;                  \
        which "clang-format-$CLANG_FORMAT_DEFAULT_VERSION"      \
        || which "clang-$CLANG_FORMAT_DEFAULT_VERSION-format"   \
        || which clang-format
    )}"
if [ ! "$CLANG_FORMAT" ]; then
    printf '\033[31m[ERROR] Clang Format (%d) has not been installed.\033[0m\n' "$CLANG_FORMAT_DEFAULT_VERSION" >&2
    exit 1
else
    CLANG_FORMAT_VERSION="$(set -e; \
        "$CLANG_FORMAT" --version \
        | tr '[:upper:]' '[:lower:]' \
        | sed -n 's/.*clang-format[[:space:]][[:space:]]*version[[:space:]][[:space:]]*\([0-9][0-9\.]*\).*/\1/pi' \
        | cut -d. -f1)"
    if [ ! "$CLANG_FORMAT_VERSION" ]; then
        printf '\033[31m[ERROR] "%s" is not a valid clang-format.\033[0m\n' "$CLANG_FORMAT" >&2
        exit 1
    elif [ "$CLANG_FORMAT_VERSION" != "$CLANG_FORMAT_DEFAULT_VERSION" ]; then
        printf '\033[33m[WARNING] Detected Clang Format version (%d) differs from the default (%d), which may provide different outcome.\033[0m\n' \
            "$CLANG_FORMAT_VERSION" \
            "$CLANG_FORMAT_DEFAULT_VERSION" \
        >&2
    fi
fi

for cmd in find grep python3 sed xargs; do
    ! which "$cmd" >/dev/null || continue
    printf '\033[31m[ERROR] Missing command "%s", you may need to install it.\033[0m\n' "$cmd" >&2
    exit 1
done

# CRLF -> LF.
# This should not be used on Windows.
! which dos2unix >/dev/null 2>&1                                                                        \
|| ls -1A                                                                                               \
| grep -v '^\.git$'                                                                                     \
| grep -v '^\.gitignore$'                                                                               \
| sed 's/\r//g'                                                                                         \
| tr '\n' '\0'                                                                                          \
| $(which parallel >/dev/null 2>&1 && echo "parallel -j$(nproc) -m" || echo 'xargs -rI{}') -0           \
    find {} -type f                                                                                     \
| sed 's/\r//g'                                                                                         \
| tr '\n' '\0'                                                                                          \
| $(which parallel >/dev/null 2>&1 && echo "parallel -j$(nproc) -m" || echo 'xargs -r'   ) -0           \
    grep -HI "$(printf '\r$')"                                                                          \
| cut -d: -f1                                                                                           \
| grep -v $(set -e;                                                                                     \
    which git >/dev/null                                                                                \
    && git submodule foreach -q pwd                                                                     \
    | xargs -rI{} realpath --relative-to="$(pwd)" {}                                                    \
    | sed 's/\/*$/\//'                                                                                  \
    | sed 's/\([\\\/\.\-]\)/\\\1/g'                                                                     \
    | sed 's/^/\-e\^/'                                                                                  \
    | grep .                                                                                            \
    || echo '^$')                                                                                       \
| sort -u                                                                                               \
| sed 's/\r//g'                                                                                         \
| tr '\n' '\0'                                                                                          \
| $(which parallel >/dev/null 2>&1 && echo "parallel -j$(nproc) -m" || echo 'xargs -r'   ) -0 dos2unix -b -v

# If Python formatter is not installed, install it
python3 -c "import black" &> /dev/null || python3 -m pip install -U black
python3 -c "import isort" &> /dev/null || python3 -m pip install -U isort
find . $(find . -maxdepth 1 -name .gitmodules -type f                           \
         | xargs -r sed -n 's/^[[:space:]]*path[[:space:]]*=[[:space:]]*//p'    \
         | grep -v '[[:space:]]'                                                \
         | cat - <(printf '%s\n' .git build out run tmp)                        \
         | xargs -r printf ' -path ./%s -prune -o')                             \
    -type f                                                                     \
| sort -u                                                                       \
| tr '\n' '\0'                                                                  \
| $(which parallel >/dev/null && echo "parallel -j$(nproc) -k -q" || echo 'xargs -rI{}') -0 bash -c 'set -e
    FILE_NAME="{}"
    case "$FILE_NAME" in
    *.c | *.cc | *.cpp | *.h | *.hpp | *.proto)
        '"'$CLANG_FORMAT'"' -i "$FILE_NAME"
        ;;
    *.py)
        python3 -m black -q "$FILE_NAME"
        python3 -m isort -q "$FILE_NAME"
        ;;
    esac
'
