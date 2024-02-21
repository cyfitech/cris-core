export SHELL = /bin/bash
export OS = $(shell uname -s)
export DIR = $(shell pwd)

export MKDIR = mkdir -p
export RM = rm -rf
export REALPATH = $(shell [ "_$(OS)" != '_Darwin' ] || printf g)realpath

export DOCKER_IMAGE ?= cajunhotpot/cris-build-debian11:20240221

export PYTHON_EXECUTABLE ?= python3

export CMD ?= "$(PYTHON_EXECUTABLE)" -m pip config --user set global.index-url "$$(set -e; ROOT_DIR=/etc/roaster/scripts . /etc/roaster/scripts/geo/pip-mirror.sh >&2; printf "%s" "$$PIP_INDEX_URL")"; bash -i

export OPT ?=
export BAZEL_OPTS ?=

.PHONY: all
all: ci

.PHONY: pull
pull:
	$$([ -w '/var/run/docker.sock' ] || echo sudo) docker pull '$(DOCKER_IMAGE)'

.PHONY: env
env:
	set -e;                                                                 \
	! which git >/dev/null 2>&1                                             \
	|| ! git rev-parse --git-dir >/dev/null 2>&1                            \
	|| repo="$$($(REALPATH) -e "$$(git rev-parse --git-dir)" | sed 's/$$/\//' | sed 's/\(.*\)\/\.git\/.*/\1/')";    \
	[ "$$repo" ] || repo="$$(pwd)";                                         \
	vol_cache="cache_$$(tr '[:punct:]' '_' <<< '$(DOCKER_IMAGE)')";         \
	sudo_docker="$$([ -w '/var/run/docker.sock' ] || echo sudo) docker";    \
	for prefix in {bazel,ccache,pip}_; do                                   \
	    $$sudo_docker volume ls -qf name="^$$prefix$$vol_cache"'$$'         \
	    | grep . >/dev/null                                                 \
	    || $$sudo_docker volume create "$$prefix$$vol_cache";               \
	done;                                                                   \
	$$sudo_docker run                                                       \
	    --cap-add=SYS_PTRACE                                                \
	    --rm                                                                \
	    --security-opt seccomp=unconfined                                   \
	    $$([ ! -d "$$repo/.git/modules" ] || find "$$($(REALPATH) -e "$$repo")/.git/modules" -name 'lfs' -type d | grep -v '[[:space:]]' | xargs -rI{} printf '%s %s' '--tmpfs' {}) \
	    $$([ ! "$$HTTP_PROXY"  ] || grep '[[:space:]]' <<< "$$HTTP_PROXY"  >/dev/null || echo "-e  HTTP_PROXY=$$HTTP_PROXY" )   \
	    $$([ ! "$$HTTPS_PROXY" ] || grep '[[:space:]]' <<< "$$HTTPS_PROXY" >/dev/null || echo "-e HTTPS_PROXY=$$HTTPS_PROXY")   \
	    $$([ ! "$$http_proxy"  ] || grep '[[:space:]]' <<< "$$http_proxy"  >/dev/null || echo "-e  http_proxy=$$http_proxy" )   \
	    $$([ ! "$$https_proxy" ] || grep '[[:space:]]' <<< "$$https_proxy" >/dev/null || echo "-e https_proxy=$$https_proxy")   \
	    $$([ ! "$$ALL_PROXY"   ] || grep '[[:space:]]' <<< "$$ALL_PROXY"   >/dev/null || echo "-e   ALL_PROXY=$$ALL_PROXY"  )   \
	    -it                                                                 \
	    $$([ -d ~/.ssh ] && printf '%s' '-v ' || printf '%s' '-l UNUSED_VOL_SSH=')"$$(printf '%s:%s:ro' {"$$HOME",'/root'}'/.ssh')" \
	    -v  "bazel_$$vol_cache:/root/.cache/bazel"                          \
	    -v "ccache_$$vol_cache:/root/.ccache"                               \
	    -v    "pip_$$vol_cache:/root/.cache/pip"                            \
	    -v "$$(printf '%s:%s:ro' {,}"$$($(REALPATH) -e "$$repo")")"         \
	    $$([ -d 'run' ] && printf '%s' '-v ' || printf '%s' '-l UNUSED_VOL_RUN=')"$$(printf '%s:%s' {,}"$$($(REALPATH) -m 'run')")" \
	    $$([ -d 'tmp' ] && printf '%s' '-v ' || printf '%s' '-l UNUSED_VOL_TMP=')"$$(printf '%s:%s' {,}"$$($(REALPATH) -m 'tmp')")" \
	    -w "$$($(REALPATH) -e .)"                                           \
	    '$(DOCKER_IMAGE)'                                                   \
	    bash -cl ":;                                                        \
	            set -e;                                                     \
	            if [ '$$([ ! -L "$$(pwd)" ] || printf 'symlink')' ]; then   \
	                mkdir -p "$$(dirname "$$(pwd)")";                       \
	                ln -sf '$$($(REALPATH) -e .)' '$$(pwd)';                \
	            fi;                                                         \
	            cd '$$(pwd)';                                               \
	            cd .;                                                       \
	        "'$(CMD)'

.PHONY: ci
ci:
	make env CMD='env && time make lint build test' DOCKER_IMAGE='$(DOCKER_IMAGE)'

.PHONY: lint
lint: scripts/format_all.sh
	$<

.PHONY: tidy
tidy: scripts/bazel_wrapper.sh | lint
	PYTHON_BIN_PATH="$$(set -e;                     \
	    printf '%s' "$(PYTHON_EXECUTABLE)"          \
	    | grep '/' >/dev/null                       \
	    && [ -x "$(PYTHON_EXECUTABLE)" ]            \
	    && $(REALPATH) -e "$(PYTHON_EXECUTABLE)"    \
	    || which "$(PYTHON_EXECUTABLE)")"           \
	$< build                                        \
	    --config=prof                               \
	    --config=lint                               \
	    $(OPT)                                      \
	    $(BAZEL_OPTS)                               \
	    $$(set -e;                                  \
	            printf '%s ' $(OPT) "$(BAZEL_OPTS)" \
	            | grep                              \
	                -e{'[[:space:]]','^'}{,'@[[:alnum:]_\.\-][[:alnum:]_\.\-]*'}'//[[:alnum:]/:_\.\-][[:alnum:]/:_\.\-]*'{'[[:space:]]','$$'}   \
	            > /dev/null                         \
	            || printf '//...'                   \
	        )

.PHONY: build
build: scripts/bazel_wrapper.sh scripts/distro_cc.sh
	PYTHON_BIN_PATH="$$(set -e;                     \
	    printf '%s' "$(PYTHON_EXECUTABLE)"          \
	    | grep '/' >/dev/null                       \
	    && [ -x "$(PYTHON_EXECUTABLE)" ]            \
	    && $(REALPATH) -e "$(PYTHON_EXECUTABLE)"    \
	    || which "$(PYTHON_EXECUTABLE)")"           \
	$< build                                        \
	    --config=prof                               \
	    --config=rel                                \
	    $(OPT)                                      \
	    $(BAZEL_OPTS)                               \
	    $$(set -e;                                  \
	            printf '%s ' $(OPT) "$(BAZEL_OPTS)" \
	            | grep                              \
	                -e{'[[:space:]]','^'}{,'@[[:alnum:]_\.\-][[:alnum:]_\.\-]*'}'//[[:alnum:]/:_\.\-][[:alnum:]/:_\.\-]*'{'[[:space:]]','$$'}   \
	            > /dev/null                         \
	            || printf '//...'                   \
	        )

.PHONY: test
test: scripts/bazel_wrapper.sh scripts/distro_cc.sh
	PYTHON_BIN_PATH="$$(set -e;                         \
	    printf '%s' "$(PYTHON_EXECUTABLE)"              \
	    | grep '/' >/dev/null                           \
	    && [ -x "$(PYTHON_EXECUTABLE)" ]                \
	    && $(REALPATH) -e "$(PYTHON_EXECUTABLE)"        \
	    || which "$(PYTHON_EXECUTABLE)")"               \
	$< test                                             \
	    --config=prof                                   \
	    --config=rel                                    \
	    --test_output=errors                            \
	    $$(set -e;                                      \
	            printf '%s ' $(OPT) "$(BAZEL_OPTS)"     \
	            | grep                                  \
	                -e{'[[:space:]]','^'}'\-t'{'[[:space:]]','$$'}  \
	                -e{'[[:space:]]','^'}'\-\-'{,'no'}'cache_test_results'{'[[:space:]]','$$'}  \
	            > /dev/null                             \
	            || printf '%s' '--nocache_test_results' \
	        )                                           \
	    $(OPT)                                          \
	    $(BAZEL_OPTS)                                   \
	    $$(set -e;                                      \
	            printf '%s ' $(OPT) "$(BAZEL_OPTS)"     \
	            | grep                                  \
	                -e{'[[:space:]]','^'}{,'@[[:alnum:]_\.\-][[:alnum:]_\.\-]*'}'//[[:alnum:]/:_\.\-][[:alnum:]/:_\.\-]*'{'[[:space:]]','$$'}   \
	            > /dev/null                             \
	            || printf '//...'                       \
	        )

.PHONY: quicktest
quicktest:
	@make test BAZEL_OPTS='--cache_test_results $(BAZEL_OPTS)'

.PHONY: sync
sync: scripts/bazel_pull.sh
	$<

.PHONY: docker
docker: docker/Dockerfile
	set -e;                                                                 \
	sudo_docker="$$([ -w '/var/run/docker.sock' ] || echo sudo) docker";    \
	$$sudo_docker build                                                     \
	    --no-cache                                                          \
	    --pull                                                              \
	    -f $<                                                               \
	    -t "$$(cut -d: -f1 <<< "$$DOCKER_IMAGE:"):local"                    \
	    .;

.PHONY: cacheclean
cacheclean:
	set -e;                                                                     \
	sudo_docker="$$([ -w '/var/run/docker.sock' ] || echo sudo) docker";        \
	echo '$(DOCKER_IMAGE)' 'cajunhotpot/cris-build'                             \
	| tr '[:punct:]' '_'                                                        \
	| xargs -rn1                                                                \
	| xargs -rI{} $$sudo_docker volume ls -qf 'name=^[[:alnum:]]+_cache_{}'     \
	| xargs -r $$sudo_docker volume rm -f;

.PHONY: clean
clean:
	$(RM) run tmp
