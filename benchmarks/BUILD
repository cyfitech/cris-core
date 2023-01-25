load("//bazel:rules.bzl", "cris_cc_test")

cris_cc_test(
    name = "benchmark",
    srcs = [
            "benchmark.cc"
        ] + glob([
            "*_benchmark.hpp",
        ]),
    deps = [
        "//:sched",
        "//:timer",
        "//:utils",
    ],
    linkopts = ["-lbenchmark"],
    env = {
        # Mute INFO logs.
        "GLOG_minloglevel": "1",
    },
)
