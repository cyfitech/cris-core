#!/bin/bash

set -e

pushd "$(dirname "$0")/.."

. scripts/distro_cc.sh

# Old version of ccache (e.g. ccache 3 shipped in Ubuntu 20.04) may have low hit rate without this.
[ "$CCACHE_SLOPPINESS" ]                                                                                                    \
|| [ "$(ccache --version | sed -n 's/.*[[:space:]]version[[:space:]]\([0-9][0-9]*\)\(\.[0-9\.]*[0-9]\).*/\1/p')" -ge 4 ]    \
|| export CCACHE_SLOPPINESS='include_file_ctime,include_file_mtime'

if which ccache >/dev/null 2>&1 && ([ -d 'run' ] && [ -w 'run' ] || [ -w '.' ]); then
    rm -rf 'run/toolchain'
    mkdir -p 'run/toolchain'
    export CR_CCACHE_CC_DIR="$(realpath -e "run/toolchain")"

    for compiler in $CC $CXX; do
        # Bazel may dereference the ccache symlink, so we use wrapper scripts instead.
        # See https://github.com/bazelbuild/rules_cc/issues/130
        cat << ____________EOF | sed 's/^            //' > "$CR_CCACHE_CC_DIR/$compiler"
            #!/bin/bash
            '$(which ccache)' '$(which $compiler)' "\$@"
____________EOF
        chmod +x "$CR_CCACHE_CC_DIR/$compiler"
    done
fi

popd
