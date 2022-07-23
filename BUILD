package(default_visibility = ["//visibility:public"])

load("//bazel:rules.bzl", "cris_cc_library")

cris_cc_library (
    name = "utils",
    srcs = glob(["src/utils/**/*.cc"]),
    hdrs = glob(["src/utils/**/*.h"]),
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    deps = [
        "@com_github_google_glog//:glog",
    ],
)

cris_cc_library (
    name = "sched",
    srcs = glob(["src/sched/**/*.cc"]),
    hdrs = glob(["src/sched/**/*.h"]),
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    deps = [
        ":utils",
    ],
)

cris_cc_library (
    name = "msg",
    srcs = glob(["src/msg/**/*.cc"]),
    hdrs = glob(["src/msg/**/*.h"]),
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    deps = [
        ":utils",
        ":sched",
    ],
)

cris_cc_library (
    name = "timer",
    srcs = glob(["src/timer/**/*.cc"]),
    hdrs = glob(["src/timer/**/*.h"]),
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    deps = [
        ":utils",
    ],
)

cris_cc_library (
    name = "signal",
    srcs = glob(["src/signal/**/*.cc"]),
    hdrs = glob(["src/signal/**/*.h"]),
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    copts = [
        "-DBOOST_STACKTRACE_USE_ADDR2LINE",
    ] + select({
        "@platforms//os:osx" : ["-DBOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED"],
        "//conditions:default": [],
    }),
    linkopts = [
        "-ldl",
    ],
    deps = [
        ":utils",
        ":timer",
        "@com_github_google_glog//:glog",
    ],
)

cris_cc_library (
    name = "config",
    srcs = glob(["src/config/**/*.cc"]),
    hdrs = glob(["src/config/**/*.h"]),
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    deps = [
        ":utils",
        "@simdjson//:libsimdjson",
    ],
)
