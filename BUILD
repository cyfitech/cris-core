package(default_visibility = ["//visibility:public"])

cc_library (
    name = "corelib",
    srcs = glob(["src/**/*.cc"]),
    includes = ["includes"],
    include_prefix = "cris/core",
    strip_include_prefix = "includes",
    hdrs = glob(["includes/**/*.h"]),
    copts = [
        "-Werror",
        "-Wall",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@libunwind//:libunwind",
    ],
)
