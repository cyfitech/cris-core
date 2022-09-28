package(default_visibility = ["//visibility:public"])

load("//bazel:rules.bzl", "cris_cc_binary")

cris_cc_binary(
    name = "ldb_record_convert",
    srcs = ["ldb_record_convert.cc"],
    deps = [
        "//:msg_recorder",
        "//:utils",
        "@com_github_gflags_gflags//:gflags",
    ],
)
