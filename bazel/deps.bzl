load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")

def cris_deps_bazel_skylib(prefix = "."):
    if not native.existing_rule("bazel_skylib"):
        native.local_repository(
            name = "bazel_skylib",
            path = prefix + "/external/bazel-skylib",
        )

def cris_deps_cmake_src(prefix = "."):
    if not native.existing_rule("cmake_src"):
        native.new_local_repository(
            name = "cmake_src",
            path = prefix + "/external/cmake",
            build_file_content = """
filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)
            """
        )

# Specific version of protobuf from git tag of rules_proto.
def cris_deps_com_github_protocolbuffers_protobuf(prefix = "."):
    if not native.existing_rule("com_github_protocolbuffers_protobuf"):
        native.local_repository(
            name = "com_github_protocolbuffers_protobuf",
            path = prefix + "/external/com_github_protocolbuffers_protobuf/protobuf",
        )

def cris_deps_gnumake_src(prefix = "."):
    if not native.existing_rule("gnumake_src"):
        native.new_local_repository(
            name = "gnumake_src",
            path = prefix + "/external/make",
            build_file_content = """
filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)
            """
        )

def cris_deps_ninja_build_src(prefix = "."):
    if not native.existing_rule("ninja_build_src"):
        native.new_local_repository(
            name = "ninja_build_src",
            path = prefix + "/external/ninja",
            build_file_content = """
filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)
            """
        )

def cris_deps_rules_cc(prefix = "."):
    if not native.existing_rule("rules_cc"):
        native.local_repository(
            name = "rules_cc",
            path = prefix + "/external/rules_cc",
        )

def cris_deps_rules_foreign_cc(prefix = ".", register_built_tools=False):
    if not native.existing_rule("rules_foreign_cc"):
        native.local_repository(
            name = "rules_foreign_cc",
            path = prefix + "/external/rules_foreign_cc",
        )
        if register_built_tools:
            cris_deps_cmake_src(prefix)
            cris_deps_gnumake_src(prefix)
            cris_deps_ninja_build_src(prefix)

def cris_deps_rules_java(prefix = "."):
    if not native.existing_rule("rules_java"):
        native.local_repository(
            name = "rules_java",
            path = prefix + "/external/rules_java",
        )

def cris_deps_rules_pkg(prefix = "."):
    if not native.existing_rule("rules_pkg"):
        native.local_repository(
            name = "rules_pkg",
            path = prefix + "/external/rules_pkg",
        )

def cris_deps_rules_proto(prefix = "."):
    if not native.existing_rule("rules_proto"):
        native.local_repository(
            name = "rules_proto",
            path = prefix + "/external/rules_proto",
        )
    cris_deps_com_github_protocolbuffers_protobuf(prefix)

def cris_deps_rules_python(prefix = "."):
    if not native.existing_rule("rules_python"):
        native.local_repository(
            name = "rules_python",
            path = prefix + "/external/rules_python",
        )

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

config_setting(
    name = "dbg_build",
    values = {
        "compilation_mode": "dbg",
    },
    visibility = ["//visibility:private"],
)

cmake(
    name = "libfmt",
    lib_source = ":all_content",
    cache_entries = {
        "CMAKE_C_FLAGS" : "-Wno-fuse-ld-path",
        "CMAKE_CXX_FLAGS" : "-Wno-fuse-ld-path",
        "CMAKE_POSITION_INDEPENDENT_CODE": "ON",
        "FMT_MASTER_PROJECT": "OFF",
        "FMT_INSTALL": "ON",
    },
    out_static_libs = select({
        ":dbg_build":           ["libfmtd.a"],
        "//conditions:default": ["libfmt.a"],
    }),
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

def cris_deps_libbacktrace(prefix = "."):
    if not native.existing_rule("libbacktrace"):
        native.new_local_repository(
            name = "libbacktrace",
            path = prefix + "/external/libbacktrace",
            build_file_content = """
load("@bazel_skylib//lib:selects.bzl", "selects")
load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

config_setting(
    name = "use_clang",
    values = {
        "compiler": "clang",
    },
)

selects.config_setting_group(
    name = "linux_use_clang",
    match_all = [
        "@platforms//os:linux",
        "use_clang",
    ]
)

filegroup(
    name = "all_content",
    srcs = glob(["**"]),
    visibility = ["//visibility:private"],
)

configure_make(
    name = "libbacktrace",
    lib_source = ":all_content",
    # Missing stack trace when using clang LTO.
    # - https://github.com/cyfitech/cris-core/issues/95#issuecomment-1441056572
    copts = select({
            "linux_use_clang":      ["-fno-lto"],
            "//conditions:default": [],
        }),
    linkopts = select({
            "linux_use_clang":      ["-fno-lto"],
            "//conditions:default": [],
        }),
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
    cris_deps_bazel_skylib(prefix)
    cris_deps_rules_cc(prefix)
    cris_deps_rules_foreign_cc(prefix)
    cris_deps_rules_java(prefix)
    cris_deps_rules_pkg(prefix)
    cris_deps_rules_proto(prefix)
    cris_deps_rules_python(prefix)

    cris_deps_bazel_clang_tidy(prefix)
    cris_deps_gflags(prefix)
    cris_deps_glog(prefix)
    cris_deps_gtest(prefix)
    cris_deps_fmt(prefix)
    cris_deps_simdjson(prefix)
    cris_deps_libbacktrace(prefix)
