package(default_visibility = ["//visibility:public"])

load("//bazel:rules.bzl", "cris_cc_library")

cris_cc_library (
    name = "corelib",
    srcs = glob(["src/**/*.cc"]),
    includes = ["includes"],
    include_prefix = "cris/core",
    strip_include_prefix = "includes",
    hdrs = glob(["includes/**/*.h"]),
    copts = [
        "-DBOOST_STACKTRACE_USE_ADDR2LINE",
    ],
    linkopts = [
        "-ldl",
    ],
    deps = [
        "@com_github_google_glog//:glog",
    ],
)
