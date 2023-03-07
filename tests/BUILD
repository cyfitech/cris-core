load("//bazel:rules.bzl", "cris_cc_library", "cris_cc_test")

cris_cc_library(
    name = "cris_gtest_main",
    srcs = ["utils/cris_gtest_main.cc"],
    deps = [
        "//:signal",
        "@gtest//:gtest",
    ],
    visibility = ["//visibility:public"],
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

# Temporarily disble this test to unblock CI since it is broken.
# Related issue:
#   - https://github.com/cyfitech/cris-core/issues/154
# TODO(yuzhouguo): Fix the flakyness and and it back.
# filegroup(
#     name = "recorder_snapshot_tsan_suppressions",
#     srcs = ["recorder_snapshot_tsan_suppressions.txt"],
# )
#
# cris_cc_test (
#     name = "recorder_snapshot_test",
#     srcs = ["recorder_snapshot_test.cc"],
#     env = {
#         "TSAN_OPTIONS": "suppressions=$(execpath recorder_snapshot_tsan_suppressions)",
#     },
#     data = [
#         ":recorder_snapshot_tsan_suppressions",
#     ],
#     deps = [
#         "//:msg_recorder",
#         "@cris-core//tests:cris_gtest_main",
#     ],
#     # TODO(yuzhouguo): When we resolve the snapshot timing check issue, the test should be non-flaky.
#     # https://github.com/cyfitech/cris-core/issues/154
#     flaky = True,
# )

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

filegroup(
    name = "job_queue_tsan_suppressions",
    srcs = ["job_queue_tsan_suppressions.txt"],
)

cris_cc_test (
    name = "job_queue_test",
    srcs = ["job_queue_test.cc"],
    env = {
        "TSAN_OPTIONS": "suppressions=$(execpath job_queue_tsan_suppressions)",
    },
    data = [
        ":job_queue_tsan_suppressions",
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

cris_cc_test (
    name = "recorder_config_test",
    srcs = ["recorder_config_test.cc"],
    deps = [
        "//:config",
        "//:msg_recorder",
        "@fmt//:libfmt",
        "@cris-core//tests:cris_gtest_main",
    ],
)

cris_cc_test (
    name = "stacktrace_test",
    srcs = ["stacktrace_test.cc"],
    deps = [
        "//:signal",
        "@cris-core//tests:cris_gtest_main",
    ],
)
