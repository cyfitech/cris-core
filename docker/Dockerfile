ARG BASE_IMAGE=ubuntu:22.04
FROM "$BASE_IMAGE" AS base

FROM base AS bootstrap

ARG TARGETARCH

SHELL ["/bin/bash", "-c"]

ENV LANG=C.UTF-8

RUN set -e; \
    export DEB_MAX_ATTEMPT=10; \
    . /etc/os-release; \
    case "$ID-$VERSION_ID" in \
    'debian-'* | 'linuxmint-'* | 'ubuntu-'*) \
        for attempt in $(seq "$DEB_MAX_ATTEMPT" -1 0); do \
            [ "$attempt" -gt 0 ]; \
            ( \
                set -e; \
                apt-get update -y; \
                DEBIAN_FRONTEND=noninteractive apt-get upgrade -y; \
                DEBIAN_FRONTEND=noninteractive apt-get install -y sudo; \
                sudo DEBIAN_FRONTEND=noninteractive apt-get install -y sed; \
            ) && break; \
            echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
        done; \
        ;; \
    esac; \
    truncate -s0 ~/.bash_history;

# - On Ubuntu 24.04 dstat/pcp cannot be installed noninteractively after
#   installed software-properties-common.
#   Install it first as a workaround.
#   https://bugs.launchpad.net/ubuntu/+source/pcp/+bug/2064784
RUN set -e; \
    export DEB_MAX_ATTEMPT=10; \
    . <(sed 's/^\(..*\)/export DISTRO_\1/' '/etc/os-release'); \
    case "$DISTRO_ID-$DISTRO_VERSION_ID" in \
    'ubuntu-24.04') \
        for attempt in $(seq "$DEB_MAX_ATTEMPT" -1 0); do \
            [ "$attempt" -gt 0 ]; \
            sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
                dstat \
            && break; \
            echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
        done; \
        ;; \
    esac; \
    case "$DISTRO_ID-$DISTRO_VERSION_ID" in \
    'debian-'* | 'linuxmint-'* | 'ubuntu-'*) \
        for attempt in $(seq "$DEB_MAX_ATTEMPT" -1 0); do \
            [ "$attempt" -gt 0 ]; \
            sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
                apt-{transport-https,utils} \
                coreutils \
                curl \
                gnupg-agent \
                grep \
                software-properties-common \
             && break; \
            echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
        done; \
        sudo apt-get autoremove -y; \
        sudo apt-get clean; \
        sudo rm -rf /var/lib/apt/lists/*; \
        ;; \
    esac; \
    truncate -s0 ~/.bash_history;

RUN set -e; \
    export DEB_MAX_ATTEMPT=10; \
    export CRIS_IMG_ARCH="$(set -e; \
        case "$(uname -m)" in \
        'amd64' | 'x64' | 'x86_64') \
            echo 'amd64'; \
            ;; \
        'aarch64' | 'arm64') \
            echo 'arm64'; \
            ;; \
        *) \
            echo "$TARGETARCH"; \
            ;; \
        esac)"; \
    . <(sed 's/^\(..*\)/export DISTRO_\1/' '/etc/os-release'); \
    case "$DISTRO_ID-$DISTRO_VERSION_ID" in \
    'debian-'*) \
        [ ! -e '/etc/apt/sources.list.d/debian.sources' ] \
        || [ -e '/etc/apt/sources.list' ] \
        || printf 'Types: deb deb-src\nURIs: http://deb.debian.org/%s\nSuites: %s\nComponents: main contrib non-free non-free-firmware\nSigned-By: /usr/share/keyrings/debian-archive-keyring.gpg\n\n' \
            'debian'          "$DISTRO_VERSION_CODENAME $DISTRO_VERSION_CODENAME-updates" \
            'debian-security' "$DISTRO_VERSION_CODENAME-security" \
        | sudo tee '/etc/apt/sources.list.d/debian.sources'; \
        for attempt in $(seq "$DEB_MAX_ATTEMPT" -1 0); do \
            [ "$attempt" -gt 0 ]; \
            ( \
                set -e; \
                sudo add-apt-repository 'contrib'; \
                sudo add-apt-repository 'non-free'; \
                sudo apt-get update -y; \
                sudo apt-get upgrade -y; \
            ) && break; \
            echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
        done; \
        ;; \
    'ubuntu-'*) \
        for attempt in $(seq "$DEB_MAX_ATTEMPT" -1 0); do \
            [ "$attempt" -gt 0 ]; \
            ( \
                set -e; \
                sudo add-apt-repository 'multiverse'; \
                sudo apt-get update -y; \
                sudo apt-get upgrade -y; \
            ) && break; \
            echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
        done; \
        ;; \
    esac; \
    case "$DISTRO_ID-$DISTRO_VERSION_ID/$CRIS_IMG_ARCH" in \
    'debian-'*'/amd64' | 'linuxmint-'*'/amd64' | 'ubuntu-'*'/amd64') \
        [ -e '/etc/apt/keyrings' ] || ! sudo mkdir -m 755 '/etc/apt/keyrings'; \
        curl --retry "$DEB_MAX_ATTEMPT" --retry-connrefused --retry-delay 3 -fsSL 'https://storage.googleapis.com/bazel-apt/doc/apt-key.pub.gpg' \
        | sudo tee /etc/apt/keyrings/bazel.asc \
        >/dev/null; \
        echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/bazel.asc] https://storage.googleapis.com/bazel-apt stable jdk1.8" \
        | sudo tee /etc/apt/sources.list.d/bazel.list; \
        sudo apt-get autoremove -y; \
        sudo apt-get clean; \
        sudo rm -rf /var/lib/apt/lists/*; \
        ;; \
    esac; \
    truncate -s0 ~/.bash_history;

FROM bootstrap as dev

RUN set -e; \
    export DEB_MAX_ATTEMPT=10; \
    export CRIS_IMG_ARCH="$(set -e; \
        case "$(uname -m)" in \
        'amd64' | 'x64' | 'x86_64') \
            echo 'amd64'; \
            ;; \
        'aarch64' | 'arm64') \
            echo 'arm64'; \
            ;; \
        *) \
            echo "$TARGETARCH"; \
            ;; \
        esac)"; \
    . <(sed 's/^\(..*\)/export DISTRO_\1/' '/etc/os-release'); \
    case "$DISTRO_ID-$DISTRO_VERSION_ID" in \
    'debian-'* | 'linuxmint-'* | 'ubuntu-'*) \
        for attempt in $(seq "$DEB_MAX_ATTEMPT" -1 0); do \
            [ "$attempt" -gt 0 ]; \
            ( \
                set -e; \
                sudo apt-get update -y; \
                sudo DEBIAN_FRONTEND=noninteractive apt-get upgrade -y; \
            ) && break; \
            echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
        done; \
        ;; \
    esac; \
    case "$DISTRO_ID-$DISTRO_VERSION_ID" in \
    'debian-'* | 'linuxmint-'* | 'ubuntu-'*) \
        for attempt in $(seq "$DEB_MAX_ATTEMPT" -1 0); do \
            [ "$attempt" -gt 0 ]; \
            sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
                auto{conf{,-archive},make,point} \
                axel \
                bash-completion \
                $( [ "_$CRIS_IMG_ARCH" != '_amd64' ] || echo 'bazel=7.*')\
                bc \
                bind9-dnsutils \
                binutils \
                bison \
                bsdmainutils \
                byacc{,-j} \
                bzip2 pbzip2 \
                ccache \
                cmake{,-curses-gui} \
                coreutils \
                curl \
                default-libmysqlclient-dev \
                diffutils \
                dos2unix \
                dstat \
                expect \
                findutils \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-2[24]\.04$' -e'^debian\-11' >/dev/null || echo g{cc,++}-10) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-2[24]\.04$' -e'^debian\-12' >/dev/null || echo g{cc,++}-11) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-2[24]\.04$' -e'^debian\-12' >/dev/null || echo g{cc,++}-12) \
                gdb \
                git{,-{extras,lfs}} \
                gzip \
                htop \
                httping \
                ifstat \
                iftop \
                $( [ "_$CRIS_IMG_ARCH" != '_amd64' ] || echo 'intel-mkl')\
                iotop \
                iproute2{,-doc} \
                'iputils-*' \
                jq \
                lib{asan6,lsan0,tsan0,ubsan1} \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-22\.04$' >/dev/null || echo lib{asan8,tsan2}) \
                lib{benchmark,boost-all,curl4-openssl,edit,eigen3,event,gflags,gmp,gtest,hdf5,hiredis,hwloc,leveldb,lmdb,lzma,mpfr,mpc,ncurses5,rocksdb,snappy,ssl,tbb}-dev \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^debian\-11$'                      >/dev/null || echo libc++{,abi}-11-dev lldb-11) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-22\.04$' -e'^debian\-12$' >/dev/null || echo libc++{,abi}-14-dev lldb-14) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-24\.04$'                  >/dev/null || echo libc++{,abi}-18-dev lldb-18) \
                liblz4-dev liblz4-tool \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^debian\-11$'     >/dev/null || echo libomp-13-dev libomp5-13) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-22\.04$' >/dev/null || echo libomp-14-dev libomp5-14) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^debian\-12$'     >/dev/null || echo libomp-16-dev libomp5-16) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-24\.04$' >/dev/null || echo libomp-18-dev libomp5-18) \
                libpapi-dev papi-tools \
                libtool \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-22\.04$'    -e'^debian\-11$'    >/dev/null || echo llvm-11{,-tools} {clang{,-{format,tidy,tools}},lld}-11) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-22\.04$'    -e'^debian\-1[12]$' >/dev/null || echo llvm-13{,-tools} {clang{,-{format,tidy,tools}},lld}-13) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-2[24]\.04$' -e'^debian\-12$'    >/dev/null || echo llvm-14{,-tools} {clang{,-{format,tidy,tools}},lld}-14) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-2[24]\.04$' -e'^debian\-12$'    >/dev/null || echo llvm-15{,-tools} {clang{,-{format,tidy,tools}},lld}-15) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-24\.04$'    -e'^debian\-1[12]$' >/dev/null || echo llvm-16{,-tools} {clang{,-{format,tidy,tools}},lld}-16) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-24\.04$'                        >/dev/null || echo llvm-17{,-tools} {clang{,-{format,tidy,tools}},lld}-17) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-24\.04$'                        >/dev/null || echo llvm-18{,-tools} {clang{,-{format,tidy,tools}},lld}-18) \
                locales \
                locate \
                lshw \
                lsof \
                liblz4-dev liblz4-tool \
                m4 \
                make \
                man \
                moreutils \
                mtr \
                net-tools \
                ninja-build \
                parallel \
                openssh-{client,server} \
                pigz \
                pkg-config \
                privoxy \
                procps \
                proxychains4 \
                pv \
                python3{,-pip} \
                rng-tools5 \
                rsync \
                ruby-dev \
                sed \
                socat \
                software-properties-common \
                sysfsutils \
                sysstat \
                tar \
                time \
                traceroute \
                tree \
                util-linux \
                uuid-{dev,runtime} \
                vim \
                wget \
                whois \
                xz-utils \
                zstd libzstd-dev \
            && break; \
            echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
        done; \
        sudo apt-get autoremove -y; \
        sudo apt-get clean; \
        sudo rm -rf /var/lib/apt/lists/*; \
        ;; \
    esac; \
    truncate -s0 ~/.bash_history;

RUN set -e; \
    echo 'en_US zh_ ja_ ko_' | xargs -n1 | sudo xargs -I{} sed -i 's/^[[:space:]]*#[[:space:]]*\({}\)/\1/' '/etc/locale.gen'; \
    sudo locale-gen; \
    truncate -s0 ~/.bash_history;

ENV LANG=en_US.UTF-8

RUN set -e; \
    git clone https://github.com/xkszltl/roaster.git /etc/roaster/scripts; \
    truncate -s0 ~/.bash_history;

# [PEP 668](https://peps.python.org/pep-0668/) disallows installing pip packages directly to the base environment.
# However, there are no viable alternatives, so we set `--break-system-package` as a workaround.
#
# HACK: In Ubuntu 24.04, the RECORD file of some packages like `wheel` and `pip` was dropped
# so it is not upgradable. We added `-I` to override the preinstalled version.
# https://bugs.launchpad.net/ubuntu/+source/wheel/+bug/2063151
RUN set -e; \
    . <(sed 's/^\(..*\)/export DISTRO_\1/' '/etc/os-release'); \
    PIP_MAYBE_BREAK_SYSTEM_PACKAGES="$(set -e; \
        OPT="--break-system-packages"; \
        python3 -m pip install --help \
        | sed 's/^/ /' \
        | sed 's/$/ /' \
        | grep -q "[^[:alnum:]_-]$OPT[^[:alnum:]_-]" \
        && printf '%s' "$OPT" \
        || true)"; \
    PIP_INDEX_URL="$(ROOT_DIR=/etc/roaster/scripts . /etc/roaster/scripts/geo/pip-mirror.sh >&2 && printf '%s' "$PIP_INDEX_URL")"; \
    export PIP_MAX_ATTEMPT=3; \
    case "$DISTRO_ID-$DISTRO_VERSION_ID" in \
    'ubuntu-24.04') \
        for attempt in $(seq "$PIP_MAX_ATTEMPT" -1 0); do \
            [ "$attempt" -gt 0 ]; \
            sudo PIP_INDEX_URL="$PIP_INDEX_URL" python3 -m pip install -IU $PIP_MAYBE_BREAK_SYSTEM_PACKAGES \
                pip \
                wheel \
            && break; \
            echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
        done; \
        ;; \
    esac; \
    for attempt in $(seq "$PIP_MAX_ATTEMPT" -1 0); do \
        [ "$attempt" -gt 0 ]; \
        sudo PIP_INDEX_URL="$PIP_INDEX_URL" python3 -m pip install -U $PIP_MAYBE_BREAK_SYSTEM_PACKAGES \
            pip \
            wheel \
            setuptools \
        && break; \
        echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
    done; \
    for attempt in $(seq "$PIP_MAX_ATTEMPT" -1 0); do \
        [ "$attempt" -gt 0 ]; \
        sudo PIP_INDEX_URL="$PIP_INDEX_URL" python3 -m pip install -U $PIP_MAYBE_BREAK_SYSTEM_PACKAGES \
            black \
            flake8 \
            isort \
            mypy \
        && break; \
        echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
    done; \
    for attempt in $(seq "$PIP_MAX_ATTEMPT" -1 0); do \
        [ "$attempt" -gt 0 ]; \
        sudo PIP_INDEX_URL="$PIP_INDEX_URL" python3 -m pip install -U $PIP_MAYBE_BREAK_SYSTEM_PACKAGES \
            h5py \
            matplotlib \
            numba \
            numpy \
            pandas \
            pytest \
            pytest-xdist \
            scikit_learn \
            scipy \
            torch \
            xgboost \
        && break; \
        echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
    done; \
    rm -rf ~/.cache/pip; \
    truncate -s0 ~/.bash_history;

RUN set -e; \
    export CRIS_IMG_ARCH="$(set -e; \
        case "$(uname -m)" in \
        'amd64' | 'x64' | 'x86_64') \
            echo 'amd64'; \
            ;; \
        'aarch64' | 'arm64') \
            echo 'arm64'; \
            ;; \
        *) \
            echo "$TARGETARCH"; \
            ;; \
        esac)"; \
    /etc/roaster/scripts/setup.sh \
        fpm \
        "$([ "_$CRIS_IMG_ARCH" = '_amd64' ] && echo 'ccache')" \
        ; \
    truncate -s0 ~/.bash_history;

RUN set -e; \
    export CRIS_IMG_ARCH="$(set -e; \
        case "$(uname -m)" in \
        'amd64' | 'x64' | 'x86_64') \
            echo 'amd64'; \
            ;; \
        'aarch64' | 'arm64') \
            echo 'arm64'; \
            ;; \
        *) \
            echo "$TARGETARCH"; \
            ;; \
        esac)"; \
    if [ "_$CRIS_IMG_ARCH" = '_arm64' ] ; then \
        BAZEL_INSTALL_DIR='/usr/local/bin'; \
        BAZEL_VERSION='7.4.0'; \
        for retry in $(seq 30 -1 1); do \
            curl \
                -fsSL \
                --retry 100 \
                --retry-connrefused \
                --retry-delay 1 \
                -o bazel \
                "https://github.com/bazelbuild/bazel/releases/download/$BAZEL_VERSION/bazel-$BAZEL_VERSION-linux-arm64" \
            && break; \
            rm -f bazel; \
            [ "$retry" -gt 1 ] && echo "Retry. $(expr "$retry" - 1) time(s) left." && sleep 5; \
        done; \
        chmod +x bazel; \
        fpm \
            -t deb -s dir \
            -p "bazel.deb" \
            --name bazel \
            --version "$BAZEL_VERSION" \
            --architecture native \
            "bazel=$BAZEL_INSTALL_DIR/bazel"; \
        sudo DEBIAN_FRONTEND=noninteractive apt-get install -y "$(pwd)/bazel.deb"; \
        sudo apt-get autoremove -y; \
        sudo apt-get clean; \
        sudo rm -rf /var/lib/apt/lists/*; \
    fi; \
    truncate -s0 ~/.bash_history;

RUN set -e; \
    echo 'Pre-extract bazel installation.'; \
    bazel version; \
    truncate -s0 ~/.bash_history;

RUN set -e; \
    touch ~/.vimrc; \
    truncate -s0 ~/.vimrc; \
    echo 'syntax on' >> ~/.vimrc; \
    echo 'set number' >> ~/.vimrc; \
    echo 'set nocompatible' >> ~/.vimrc; \
    echo 'set backspace=indent,eol,start' >> ~/.vimrc; \
    echo 'set cindent' >> ~/.vimrc; \
    echo 'set nobackup' >> ~/.vimrc; \
    echo 'set expandtab' >> ~/.vimrc; \
    echo 'set softtabstop=4' >> ~/.vimrc; \
    echo 'set shiftwidth=4' >> ~/.vimrc; \
    echo 'set tabstop=4' >> ~/.vimrc; \
    echo 'set cinoptions=:0,g0' >> ~/.vimrc; \
    echo 'colorscheme default' >> ~/.vimrc; \
    echo 'autocmd FileType make set noexpandtab' >> ~/.vimrc; \
    truncate -s0 ~/.bash_history;

RUN set -e; \
    sudo updatedb; \
    truncate -s0 ~/.bash_history;

FROM dev as prefill

# Cache pkg list for convenience.
RUN set -e; \
    export DEB_MAX_ATTEMPT=10; \
    for attempt in $(seq "$DEB_MAX_ATTEMPT" -1 0); do \
        [ "$attempt" -gt 0 ]; \
        sudo apt-get update -y && break; \
        echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
    done; \
    truncate -s0 ~/.bash_history;
