load("//bazel:rules.bzl", "cris_cc_test", "cris_cc_library")

cris_cc_library(
    name = "cris_benchmark_main",
    srcs = ["cris_benchmark_main.cc"],
    deps = [
        "//:signal",
        "//:utils",
    ],
    linkopts = ["-lbenchmark"],
    visibility = ["//visibility:public"],
)

cris_cc_test(
    name = "job_runner_benchmark",
    srcs = ["job_runner_benchmark.cc"],
    deps = [
        ":cris_benchmark_main",
        "//:sched",
    ],
)

cris_cc_test(
    name = "time_benchmark",
    srcs = ["time_benchmark.cc"],
    deps = [
        ":cris_benchmark_main",
        "//:utils",
    ],
)

cris_cc_test(
    name = "timer_benchmark",
    srcs = ["timer_benchmark.cc"],
    deps = [
        ":cris_benchmark_main",
        "//:timer",
    ],
)
