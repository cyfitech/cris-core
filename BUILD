package(default_visibility = ["//visibility:public"])

load("//bazel:rules.bzl", "cris_cc_library")
load("@bazel_skylib//lib:selects.bzl", "selects")

config_setting(
    name = "use_clang",
    values = {
        "compiler": "clang",
    },
)

config_setting(
    name = "use_gcc",
    values = {
        "compiler": "gcc",
    },
)

cris_cc_library (
    name = "utils",
    srcs = glob(["src/utils/**/*.cc"]),
    hdrs = glob(["src/utils/**/*.h"]),
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    copts = select({
        "@platforms//os:macos": [""],
        "//conditions:default": ["-DCRIS_USE_PAPI"],
    }),
    linkopts = select({
        "@platforms//os:macos": [],
        "//conditions:default": ["-lpapi"],
    }),
    deps = [
        "@com_github_google_glog//:glog",
        "@fmt//:libfmt",
    ],
)

# Note: Only link this to executable directly.
cris_cc_library (
    name = "supp",
    srcs = glob(["src/supp/**/*.cpp"]),
    hdrs = glob(["src/supp/**/*.h"]),
    include_prefix = "cris/core",
    strip_include_prefix = "src",
)

cris_cc_library (
    name = "sched",
    srcs = glob(["src/sched/**/*.cc"]),
    hdrs = glob(["src/sched/**/*.h"]),
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    deps = [
        ":utils",
        "@simdjson//:libsimdjson",
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
    name = "internal_msg_record_key",
    srcs = ["src/msg_recorder/record_key.cc"],
    hdrs = ["src/msg_recorder/record_key.h"],
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    deps = [
        ":utils",
    ],
    visibility = ["//visibility:private"],
)

cris_cc_library (
    name = "internal_msg_recorder_utils",
    srcs = glob(["src/msg_recorder/impl/**/*.cc"]),
    hdrs = glob(["src/msg_recorder/impl/**/*.h"]),
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    visibility = ["//visibility:private"],
)

cris_cc_library (
    name = "internal_rolling_helper",
    srcs = ["src/msg_recorder/rolling_helper.cc"],
    hdrs = ["src/msg_recorder/rolling_helper.h"],
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    deps = [
        ":internal_msg_recorder_utils",
        ":utils",
    ],
    visibility = ["//visibility:private"],
)

cris_cc_library (
    name = "internal_msg_record_file",
    srcs = ["src/msg_recorder/record_file.cc"],
    hdrs = ["src/msg_recorder/record_file.h"],
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    copts = [
        "-fno-rtti",
    ],
    linkopts = [
        "-lleveldb",
    ],
    deps = [
        ":utils",
        ":internal_msg_record_key",
        ":internal_rolling_helper",
    ],
    visibility = ["//visibility:private"],
)

cris_cc_library (
    name = "msg_recorder",
    srcs = [
        "src/msg_recorder/recorder.cc",
        "src/msg_recorder/replayer.cc",
        "src/msg_recorder/recorder_config.cc",
    ],
    hdrs = [
        "src/msg_recorder/recorder.h",
        "src/msg_recorder/replayer.h",
        "src/msg_recorder/recorder_config.h",
    ],
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    deps = [
        ":config",
        ":msg",
        ":internal_msg_record_file",
        ":internal_msg_recorder_utils",
        "@fmt//:libfmt",
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

selects.config_setting_group(
    name = "macos_use_clang",
    match_all = [
        "@platforms//os:macos",
        "use_clang",
    ]
)

selects.config_setting_group(
    name = "linux_use_gcc",
    match_all = [
        "@platforms//os:linux",
        "use_gcc",
    ]
)

selects.config_setting_group(
    name = "linux_use_clang",
    match_all = [
        "@platforms//os:linux",
        "use_clang",
    ]
)

cris_cc_library (
    name = "signal",
    srcs = glob(["src/signal/**/*.cc"]),
    hdrs = glob(["src/signal/**/*.h"]),
    include_prefix = "cris/core",
    strip_include_prefix = "src",
    copts = select({
        "@platforms//os:macos": ["-DBOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED"],
        "//conditions:default": ["-DBOOST_STACKTRACE_USE_BACKTRACE"],
    }),
    linkopts = select({
        "@platforms//os:macos": [],
        "use_gcc":              ["-ldl", "-lbacktrace"],
        "//conditions:default": ["-ldl"],
    }),
    deps = [
        ":utils",
        ":timer",
        "@com_github_google_glog//:glog",
    ] + select({
        "@platforms//os:macos": [],
        "use_gcc":              [],
        "//conditions:default": ["@libbacktrace//:libbacktrace"],
    }),
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

filegroup(
    name = "clang_tidy_config",
    srcs = [".clang-tidy"],
)
