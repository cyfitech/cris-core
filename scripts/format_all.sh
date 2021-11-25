#!/bin/bash

set -e

cd "$(dirname "$0")/.."

for cmd in clang-format find grep python3 sed xargs; do
    which "$cmd" >/dev/null
done

[ -e 'scripts/codebase_utils.sh' ] && . scripts/codebase_utils.sh || CRIS_CODE_DIRS=''

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
| sort -u                                                                                               \
| sed 's/\r//g'                                                                                         \
| tr '\n' '\0'                                                                                          \
| $(which parallel >/dev/null 2>&1 && echo "parallel -j$(nproc) -m" || echo 'xargs -r'   ) -0 dos2unix -b -v

# If Python formatter is not installed, install it
python3 -c "import black" &> /dev/null || python3 -m pip install -U black

for dir in $CRIS_CODE_DIRS; do
    # clang-format files
    printf '%s\n' '*.'{c{,c,pp},h{,pp},proto} | xargs -rI{} find "$dir" -name "{}" | sort -u | xargs -rI{} clang-format -i {}

    # python files
    python3 -m black "$dir" 2>/dev/null
done
