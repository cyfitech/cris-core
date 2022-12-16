load("//bazel:rules.bzl", "cris_cc_test")

cris_cc_test (
    name = "defs_test",
    srcs = ["defs_test.cc"],
    deps = [
        "//:utils",
        "//:custom_main",
    ],
)

cris_cc_test (
    name = "message_test",
    srcs = ["message_test.cc"],
    deps = [
        "//:msg",
        "//:custom_main",
    ],
)

cris_cc_test (
    name = "msg_recorder_test",
    srcs = ["msg_recorder_test.cc"],
    deps = [
        "//:msg_recorder",
        "//:custom_main",
    ],
    flaky = True,
)

cris_cc_test (
    name = "record_file_db_reopen_test",
    srcs = ["record_file_db_reopen_test.cc"],
    deps = [
        "//:msg_recorder",
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
        "//:custom_main",
    ],
)

cris_cc_test (
    name = "timer_test",
    srcs = ["timer_test.cc"],
    deps = [
        "//:timer",
        "//:custom_main",
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
        "//:custom_main",
    ],
)

cris_cc_test (
    name = "config_test",
    srcs = ["config_test.cc"],
    deps = [
        "//:config",
        "@fmt//:libfmt",
        "//:custom_main",
    ],
)

cris_cc_test (
    name = "record_key_test",
    srcs = ["record_key_test.cc"],
    deps = [
        "//:msg_recorder",
        "//:custom_main",
    ],
)
