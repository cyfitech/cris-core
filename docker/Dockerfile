ARG BASE_IMAGE=ubuntu:22.04
FROM "$BASE_IMAGE" AS base

FROM base AS bootstrap

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

RUN set -e; \
    export DEB_MAX_ATTEMPT=10; \
    . <(sed 's/^\(..*\)/export DISTRO_\1/' '/etc/os-release'); \
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
    . <(sed 's/^\(..*\)/export DISTRO_\1/' '/etc/os-release'); \
    case "$DISTRO_ID-$DISTRO_VERSION_ID" in \
    'debian-'*) \
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
    case "$DISTRO_ID-$DISTRO_VERSION_ID" in \
    'debian-'* | 'linuxmint-'* | 'ubuntu-'*) \
        curl --retry "$DEB_MAX_ATTEMPT" --retry-connrefused --retry-delay 3 -fsSL 'https://bazel.build/bazel-release.pub.gpg' \
        | gpg --dearmor \
        | sudo tee /etc/apt/trusted.gpg.d/bazel.gpg \
        >/dev/null; \
        echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" \
        | sudo tee /etc/apt/sources.list.d/bazel.list; \
        sudo apt-get autoremove -y; \
        sudo apt-get clean; \
        sudo rm -rf /var/lib/apt/lists/*; \
        ;; \
    esac; \
    truncate -s0 ~/.bash_history;

FROM bootstrap as dev

# Known issues:
# - MKL on Ubuntu 20.04 is too strict on libomp version.
#   Pinned explicitly to work around.
#   https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=963991
RUN set -e; \
    export DEB_MAX_ATTEMPT=10; \
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
                'bazel=5.2.*' \
                bc \
                binutils \
                bsdmainutils \
                bzip2 pbzip2 \
                ccache \
                cmake{,-curses-gui} \
                diffutils \
                dos2unix \
                findutils \
                g{cc,++}-10 \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-22\.04$' >/dev/null || echo g{cc,++}-12) \
                gdb \
                git{,-{extras,lfs}} \
                gzip \
                httping \
                intel-mkl \
                'iputils-*' \
                jq \
                lib{asan6,lsan0,tsan0,ubsan1} \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-22\.04$' >/dev/null || echo lib{asan8,tsan2}) \
                lib{boost-all,curl4-openssl,eigen3,gflags,gmp,gtest,hiredis,hwloc,mpfr,mpc,ssl,tbb}-dev \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^debian\-11$'     >/dev/null || echo libc++{,abi}-11-dev lldb-11) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-20\.04$' >/dev/null || echo libc++{,abi}-12-dev lldb-12) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-22\.04$' >/dev/null || echo libc++{,abi}-14-dev lldb-14) \
                liblz4-dev liblz4-tool \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-20\.04$' >/dev/null || echo libomp5-10) \
                libpapi-dev papi-tools \
                libtool \
                llvm-11{,-tools} {clang{,-{format,tidy,tools}},lld}-11 \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-2[02]\.04$' >/dev/null || echo llvm-12{,-tools} {clang{,-{format,tidy,tools}},lld}-12) \
                $(! echo "$DISTRO_ID-$DISTRO_VERSION_ID" | grep -e'^ubuntu\-2[24]\.04$' >/dev/null || echo llvm-14{,-tools} {clang{,-{format,tidy,tools}},lld}-14) \
                locate \
                locales \
                m4 \
                make \
                mtr \
                moreutils \
                net-tools \
                ninja-build \
                parallel \
                pigz \
                procps \
                pv \
                python3{,-pip} \
                ruby-dev \
                rsync \
                tar \
                time \
                tree \
                util-linux \
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

RUN set -e; \
    echo 'Pre-extract bazel installation.'; \
    bazel version; \
    truncate -s0 ~/.bash_history;

ENV LANG=en_US.UTF-8

RUN set -e; \
    git clone https://github.com/xkszltl/roaster.git /etc/roaster/scripts; \
    truncate -s0 ~/.bash_history;

RUN set -e; \
    PIP_INDEX_URL="$(cd '/etc/roaster/scripts' >/dev/null && bash -c '. geo/pip-mirror.sh >/dev/null && echo "$PIP_INDEX_URL"')"; \
    export PIP_MAX_ATTEMPT=3; \
    for attempt in $(seq "$PIP_MAX_ATTEMPT" -1 0); do \
        [ "$attempt" -gt 0 ]; \
        sudo PIP_INDEX_URL="$PIP_INDEX_URL" python3 -m pip install -U pip wheel setuptools && break; \
        echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
    done; \
    for attempt in $(seq "$PIP_MAX_ATTEMPT" -1 0); do \
        [ "$attempt" -gt 0 ]; \
        sudo PIP_INDEX_URL="$PIP_INDEX_URL" python3 -m pip install -U \
            black \
            flake8 \
            isort \
        && break; \
        echo "Retrying... $(expr "$attempt" - 1) chance(s) left."; \
    done; \
    rm -rf ~/.cache/pip; \
    truncate -s0 ~/.bash_history;

RUN set -e; \
    /etc/roaster/scripts/setup.sh fpm ccache; \
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
