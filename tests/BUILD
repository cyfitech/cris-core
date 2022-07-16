load("//bazel:rules.bzl", "cris_cc_test")

cris_cc_test (
    name = "defs_test",
    srcs = ["defs_test.cc"],
    deps = [
        "//:utils",
        "@gtest//:gtest_main",
    ],
)

cris_cc_test (
    name = "message_test",
    srcs = ["message_test.cc"],
    deps = [
        "//:msg",
        "@gtest//:gtest_main",
    ],
)

cris_cc_test (
    name = "node_test",
    srcs = ["node_test.cc"],
    env = {
        "TSAN_OPTIONS": "suppressions=$(execpath job_runner_tsan_suppressions)",
    },
    data = [
        ":job_runner_tsan_suppressions",
    ],
    deps = [
        "//:msg",
        "@gtest//:gtest_main",
    ],
)

cris_cc_test (
    name = "timer_test",
    srcs = ["timer_test.cc"],
    deps = [
        "//:timer",
        "@gtest//:gtest_main",
    ],
)

filegroup(
    name = "job_runner_tsan_suppressions",
    srcs = ["job_runner_tsan_suppressions.txt"],
)

cris_cc_test (
    name = "job_runner_test",
    srcs = ["job_runner_test.cc"],
    env = {
        "TSAN_OPTIONS": "suppressions=$(execpath job_runner_tsan_suppressions)",
    },
    data = [
        ":job_runner_tsan_suppressions",
    ],
    deps = [
        "//:sched",
        "@gtest//:gtest_main",
    ],
)

cris_cc_test (
    name = "config_test",
    srcs = ["config_test.cc"],
    deps = [
        "//:config",
        "@fmt//:libfmt",
        "@gtest//:gtest_main",
    ],
)
