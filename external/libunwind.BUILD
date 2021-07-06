load("@rules_foreign_cc//foreign_cc:configure.bzl", "configure_make")

filegroup(
    name = "all_content",
    srcs = glob(["**"]),
    visibility = ["//visibility:private"],
)

configure_make(
    name = "libunwind",
    lib_source = ":all_content",
    env = {
        "CFLAGS" : "-Wno-error",
    },
    visibility = ["//visibility:public"],
)
