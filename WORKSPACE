workspace(name = "cris-core")

load("@cris-core//bazel:repo.bzl", "cris_toolchains")

cris_toolchains()

load("@cris-core//bazel:deps.bzl", "cris_core_deps")
cris_core_deps()

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
bazel_skylib_workspace()

# TODO: Enable when ready.
# load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies", "rules_cc_toolchains")
# rules_cc_dependencies()
# rules_cc_toolchains()

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")
rules_foreign_cc_dependencies(register_default_tools=False, register_built_tools=False)

load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")
rules_pkg_dependencies()
