package(default_visibility = ["//visibility:public"])

load("//bazel:rules.bzl", "cr_cc_library")

cr_cc_library (
    name = "corelib",
    srcs = glob(["src/**/*.cc"]),
    includes = ["includes"],
    include_prefix = "cris/core",
    strip_include_prefix = "includes",
    hdrs = glob(["includes/**/*.h"]),
    linkopts = [
        "-lunwind",
    ],
    deps = [
        "@com_github_google_glog//:glog",
    ],
)
