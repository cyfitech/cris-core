#!/bin/bash

set -e

pushd "$(dirname "$0")/.." >/dev/null

. scripts/distro_cc.sh

# Old version of ccache (e.g. ccache 3 shipped in Ubuntu 20.04) may have low hit rate without this.
[ "$CCACHE_SLOPPINESS" ]                                                                                                    \
|| [ "$(ccache --version | sed -n 's/.*[[:space:]]version[[:space:]]\([0-9][0-9]*\)\(\.[0-9\.]*[0-9]\).*/\1/p')" -ge 4 ]    \
|| export CCACHE_SLOPPINESS='include_file_ctime,include_file_mtime'

if which ccache >/dev/null 2>&1 && ([ -d 'run' ] && [ -w 'run' ] || [ -w '.' ]); then
    rm -rf 'run/toolchain'
    mkdir -p 'run/toolchain'
    CR_CCACHE_CC_DIR="$(realpath -e "run/toolchain")"

    for compiler_varname in "CC" "CXX"; do
        compiler="$(eval echo "\$$compiler_varname")"
        if [ ! "$compiler" ]; then
            printf '\033[33m[WARNING] Missing $%s while creating ccache wrapper. Skipped.\033[0m\n' "$compiler_varname" >&2
        fi
        ccache_wrapper="$CR_CCACHE_CC_DIR/$(basename "$compiler")-wrapper"
        # Bazel build may pick the wrong file sharing the same name.
        while which "$(basename "$ccache_wrapper")" >/dev/null 2>&1; do
            printf '\033[33m[WARNING] Found name collision in PATH for ccache wrapper "%s".\033[0m\n' "$ccache_wrapper" >&2
            ccache_wrapper="$ccache_wrapper-uniq"
        done
        # Bazel may dereference the ccache symlink, so we use wrapper scripts instead.
        # See https://github.com/bazelbuild/rules_cc/issues/130
        cat << ____________EOF | sed 's/^            //' > "$ccache_wrapper"
            #!/bin/bash
            set -e
            '$(which ccache)' '$(which "$compiler")' "\$@"
____________EOF
        chmod +x "$ccache_wrapper"
        export "$compiler_varname=$ccache_wrapper"
    done
fi

popd >/dev/null

DEFAULT_CLANG_TIDY_VERSION=13
export CLANG_TIDY="${CLANG_TIDY:-$(which "clang-tidy-$DEFAULT_CLANG_TIDY_VERSION" || which clang-tidy)}"

if [ ! "$CLANG_TIDY" ]; then
    printf '\033[33m[WARNING] Clang Tidy ('"$DEFAULT_CLANG_TIDY_VERSION"') has not been installed.\033[0m\n' >&2
elif ! "$CLANG_TIDY" --dump-config | grep -i '^[[:space:]]*Checks[[:space:]]*:' > /dev/null; then
    printf '\033[31m[ERROR] "'"$CLANG_TIDY"'" is not a valid clang-tidy.\033[0m\n' >&2
    exit 1
fi
