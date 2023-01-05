load("//bazel/repo:toolchain.bzl", "cc_autoconf_toolchain")
load("//bazel/repo:backtrace.bzl", "cc_libbacktrace_header")

def cris_toolchains(*args):
    cc_autoconf_toolchain(name = "cris_toolchains")

def cris_libbacktrace(*args):
    cc_libbacktrace_header(name = "cris_libbacktrace")
