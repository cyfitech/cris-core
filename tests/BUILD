cc_test_sanitizers = [
    "-fsanitize=address",
    "-fsanitize=leak",
    "-fsanitize=undefined",
]

cc_test (
    name = "defs_test",
    srcs = ["defs_test.cc"],
    deps = [
        "//:corelib",
        "@gtest//:gtest_main",
    ],
    linkopts = cc_test_sanitizers,
)

cc_test (
    name = "message_test",
    srcs = ["message_test.cc"],
    deps = [
        "//:corelib",
        "@gtest//:gtest_main",
    ],
    linkopts = cc_test_sanitizers,
)

cc_test (
    name = "lock_queue_test",
    srcs = ["lock_queue_test.cc"],
    deps = [
        "//:corelib",
        "@gtest//:gtest_main",
    ],
    linkopts = cc_test_sanitizers,
)

cc_test (
    name = "node_test",
    srcs = ["node_test.cc"],
    deps = [
        "//:corelib",
        "@gtest//:gtest_main",
    ],
    linkopts = cc_test_sanitizers,
)

cc_test (
    name = "rr_runner_test",
    srcs = ["rr_runner_test.cc"],
    deps = [
        "//:corelib",
        "@gtest//:gtest_main",
    ],
    linkopts = cc_test_sanitizers,
)

cc_test (
    name = "timer_test",
    srcs = ["timer_test.cc"],
    deps = [
        "//:corelib",
        "@gtest//:gtest_main",
    ],
    linkopts = cc_test_sanitizers,
)
