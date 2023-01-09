load("//bazel/repo:toolchain.bzl", "cc_autoconf_toolchain")

def cris_toolchains(*args):
    cc_autoconf_toolchain(name = "cris_toolchains")

