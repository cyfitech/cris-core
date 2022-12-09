def cc_autoconf_toolchain_impl(repo_ctx):
    clang_tidy_bin_raw = repo_ctx.os.environ.get("CLANG_TIDY", "clang-tidy")
    if '/' in clang_tidy_bin_raw or '\\' in clang_tidy_bin_raw:
       clang_tidy_bin_full_path = clang_tidy_bin_raw
    else:
        clang_tidy_bin_full_path = repo_ctx.which(clang_tidy_bin_raw)
    repo_ctx.file(
        "BUILD",
        """
filegroup(
    name = "clang_tidy_wrapper",
    srcs = [\"clang_tidy_wrapper.sh\"],
)
        """,
    )
    repo_ctx.template(
        "clang_tidy_wrapper.sh",
        Label("@cris-core//bazel/repo:clang_tidy_wrapper.sh.tpl"),
        {
            "%{CLANG_TIDY}": str(clang_tidy_bin_full_path),
        },
    )

cc_autoconf_toolchain = repository_rule(
    environ = [
        "CLANG_TIDY",
    ],
    implementation = cc_autoconf_toolchain_impl,
    configure = True,
)
