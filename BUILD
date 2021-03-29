package(default_visibility = ["//visibility:public"])

cc_library (
    name = "corelib",
    srcs = glob(["src/**/*.cc"]),
    includes = ["includes"],
    hdrs = glob(["includes/**/*.h"]),
    copts = [
        "-Werror",
        "-Wall",
    ]
)
