load("//bazel:rules.bzl", "cris_cc_library", "cris_cc_test")

cris_cc_library(
    name = "cris_gtest_main",
    srcs = ["utils/cris_gtest_main.cc"],
    deps = [
        "//:signal",
        "@gtest//:gtest",
    ],
)

cris_cc_test (
    name = "defs_test",
    srcs = ["defs_test.cc"],
    deps = [
        "//:utils",
        "@cris-core//tests:cris_gtest_main",
    ],
)

cris_cc_test (
    name = "mapping_test",
    srcs = ["mapping_test.cc"],
    deps = [
        "//:utils",
        "@cris-core//tests:cris_gtest_main",
    ],
)

cris_cc_test (
    name = "message_test",
    srcs = ["message_test.cc"],
    deps = [
        "//:msg",
        "@cris-core//tests:cris_gtest_main",
    ],
)

cris_cc_test (
    name = "msg_recorder_test",
    srcs = ["msg_recorder_test.cc"],
    deps = [
        "//:msg_recorder",
        "@cris-core//tests:cris_gtest_main",
    ],
    flaky = True,
)

cris_cc_test (
    name = "record_file_db_reopen_test",
    srcs = ["record_file_db_reopen_test.cc"],
    deps = [
        "//:msg_recorder",
        "@cris-core//tests:cris_gtest_main",
    ],
)

filegroup(
    name = "recorder_snapshot_tsan_suppressions",
    srcs = ["recorder_snapshot_tsan_suppressions.txt"],
)

cris_cc_test (
    name = "recorder_snapshot_test",
    srcs = ["recorder_snapshot_test.cc"],
    env = {
        "TSAN_OPTIONS": "suppressions=$(execpath recorder_snapshot_tsan_suppressions)",
    },
    data = [
        ":recorder_snapshot_tsan_suppressions",
    ],
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
        "@cris-core//tests:cris_gtest_main",
    ],
)

cris_cc_test (
    name = "timer_test",
    srcs = ["timer_test.cc"],
    deps = [
        "//:timer",
        "@cris-core//tests:cris_gtest_main",
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
        "@cris-core//tests:cris_gtest_main",
    ],
)

cris_cc_test (
    name = "config_test",
    srcs = ["config_test.cc"],
    deps = [
        "//:config",
        "@fmt//:libfmt",
        "@cris-core//tests:cris_gtest_main",
    ],
)

cris_cc_test (
    name = "record_key_test",
    srcs = ["record_key_test.cc"],
    deps = [
        "//:msg_recorder",
        "@cris-core//tests:cris_gtest_main",
    ],
)
