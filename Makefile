export SHELL = /bin/bash
export OS = $(shell uname)
export DIR = $(shell pwd)

export MKDIR = mkdir -p
export RM = rm -rf

export CMD ?= bash
export DOCKER_IMAGE ?= cajunhotpot/cris-build-debian11:20221204

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
	|| repo="$$(realpath -e "$$(git rev-parse --git-dir)" | sed 's/$$/\//' | sed 's/\(.*\)\/\.git\/.*/\1/')";   \
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
	    $$([ ! "$$HTTP_PROXY"  ] || grep '[[:space:]]' <<< "$$HTTP_PROXY"  >/dev/null || echo "-e  HTTP_PROXY=$$HTTP_PROXY" )   \
	    $$([ ! "$$HTTPS_PROXY" ] || grep '[[:space:]]' <<< "$$HTTPS_PROXY" >/dev/null || echo "-e HTTPS_PROXY=$$HTTPS_PROXY")   \
	    $$([ ! "$$http_proxy"  ] || grep '[[:space:]]' <<< "$$http_proxy"  >/dev/null || echo "-e  http_proxy=$$http_proxy" )   \
	    $$([ ! "$$https_proxy" ] || grep '[[:space:]]' <<< "$$https_proxy" >/dev/null || echo "-e https_proxy=$$https_proxy")   \
	    $$([ ! "$$ALL_PROXY"   ] || grep '[[:space:]]' <<< "$$ALL_PROXY"   >/dev/null || echo "-e   ALL_PROXY=$$ALL_PROXY"  )   \
	    -it                                                                 \
	    $$([ ! -d ~/.ssh       ] || grep '[[:space:]]' <<< "$$HOME"        >/dev/null || echo "-v $$HOME/.ssh:/root/.ssh:ro")   \
	    -v  "bazel_$$vol_cache:/root/.cache/bazel"                          \
	    -v "ccache_$$vol_cache:/root/.ccache"                               \
	    -v    "pip_$$vol_cache:/root/.cache/pip"                            \
	    -v "$$repo:$$repo:ro"                                               \
	    $$([ ! -d 'run' ] || pwd | grep '[[:space:]]' >/dev/null || echo "-v $$(pwd)/run:$$(pwd)/run")  \
	    $$([ ! -d 'tmp' ] || pwd | grep '[[:space:]]' >/dev/null || echo "-v $$(pwd)/tmp:$$(pwd)/tmp")  \
	    -w "$$(pwd)"                                                        \
	    '$(DOCKER_IMAGE)'                                                   \
	    bash -c '$(CMD)'

.PHONY: ci
ci:
	make env CMD='env && time make lint build test' DOCKER_IMAGE='$(DOCKER_IMAGE)'

.PHONY: lint
lint: scripts/format_all.sh
	$<

.PHONY: tidy
tidy: scripts/bazel_wrapper.sh | lint
	$< build --config=prof --config=lint //...

.PHONY: build
build: scripts/bazel_wrapper.sh scripts/distro_cc.sh
	$< build //...

.PHONY: test
test: scripts/bazel_wrapper.sh scripts/distro_cc.sh
	$< test $$(. scripts/distro_cc.sh >/dev/null && "$$($(SHELL) -c 'echo "$$CC"')" --version | grep -i 'clang version' >/dev/null && echo '--config=lto') --copt='-O3' --nocache_test_results --test_output=errors //...

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
