load("//bazel:rules.bzl", "cris_cc_test")

cris_cc_test(
    name = "benchmark",
    srcs = glob(["*.cc"]),
    deps = [
        "//:sched",
        "//:signal",
        "//:timer",
        "//:utils",
    ],
    linkopts = ["-lbenchmark"],
)
