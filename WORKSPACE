workspace(name = "cris-core")

load("@cris-core//bazel:repo.bzl", "cris_toolchains")

cris_toolchains()

local_repository(
    name = "rules_foreign_cc",
    path = "external/rules_foreign_cc",
)
load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")
rules_foreign_cc_dependencies()

load("@cris-core//bazel:deps.bzl", "cris_core_deps")
cris_core_deps()
