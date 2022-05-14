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

def cris_core_deps(prefix = "."):
    cris_deps_gflags(prefix)
    cris_deps_glog(prefix)
    cris_deps_gtest(prefix)
