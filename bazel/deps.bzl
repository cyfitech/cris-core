def cris_deps_gflags(prefix = "."):
    if not native.existing_rule("gflags"):
        native.local_repository(
            name = "com_github_gflags_gflags",
            path = prefix + "/external/gflags",
        )

def cris_deps_glog(prefix = "."):
    if not native.existing_rule("glog"):
        native.local_repository(
            name = "com_github_google_glog",
            path = prefix + "/external/glog",
        )

def cris_deps_gtest(prefix = "."):
    if not native.existing_rule("gtest"):
        native.local_repository(
            name = "gtest",
            path = prefix + "/external/googletest",
        )

def cris_deps_fmt(prefix = "."):
    if not native.existing_rule("fmt"):
        native.new_local_repository(
            name = "fmt",
            path = prefix + "/external/fmt",
            build_file_content = """
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "all_content",
    srcs = glob(["**"]),
    visibility = ["//visibility:private"],
)

cmake(
    name = "libfmt",
    lib_source = ":all_content",
    cache_entries = {
        "CMAKE_C_FLAGS" : "-Wno-fuse-ld-path",
        "CMAKE_CXX_FLAGS" : "-Wno-fuse-ld-path",
        "FMT_MASTER_PROJECT": "OFF",
        "FMT_INSTALL": "ON",
    },
    generate_args = [
        "-G Ninja",
    ],
    visibility = ["//visibility:public"],
)
            """,
        )

def cris_deps_simdjson(prefix = "."):
    if not native.existing_rule("simdjson"):
        native.new_local_repository(
            name = "simdjson",
            path = prefix + "/external/simdjson",
            build_file_content = """
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "all_content",
    srcs = glob(["**"]),
    visibility = ["//visibility:private"],
)

cmake(
    name = "libsimdjson",
    lib_source = ":all_content",
    cache_entries = {
        "CMAKE_C_FLAGS" : "-Wno-fuse-ld-path",
        "CMAKE_CXX_FLAGS" : "-Wno-fuse-ld-path",
        "SIMDJSON_BUILD_STATIC": "ON",
        "SIMDJSON_EXCEPTIONS": "OFF",
        "SIMDJSON_JUST_LIBRARY": "ON",
    },
    generate_args = [
        "-G Ninja",
    ],
    visibility = ["//visibility:public"],
)
            """,
        )

def cris_deps_bazel_clang_tidy(prefix = "."):
    if not native.existing_rule("bazel_clang_tidy"):
        native.local_repository(
            name = "bazel_clang_tidy",
            path = prefix + "/external/bazel_clang_tidy",
        )

def cris_core_deps(prefix = "."):
    cris_deps_bazel_clang_tidy(prefix)
    cris_deps_gflags(prefix)
    cris_deps_glog(prefix)
    cris_deps_gtest(prefix)
    cris_deps_fmt(prefix)
    cris_deps_simdjson(prefix)
