CRIS_CXXOPTS = [
    "-Wall",
    "-Wconversion",
    "-Wextra",
    "-Wshadow",
    "-g3",
    "-fno-sanitize-recover=all",
] + select({
    # Our targeted platform is Linux so we treat warnings as errors on Linux.
    #
    # For other platforms, there may be warnings because of the CI-uncovered toolchains/libs,
    # we disbale the errors so that the developers may try it on different platforms without
    # the bothering of fixing random warnings.
    "@platforms//os:linux": [
        "-Werror",
    ],
    "//conditions:default": [],
})

CRIS_LINKOPTS = [
    "-Wall",
    "-Werror",
    "-Wextra",
    "-Wno-fuse-ld-path",
]

def __add_opts(attrs):
    if "copts" in attrs and attrs["copts"] != None:
        attrs["copts"] = CRIS_CXXOPTS + attrs["copts"]
    else:
        attrs["copts"] = CRIS_CXXOPTS

    if "linkopts" in attrs and attrs["linkopts"] != None:
        attrs["linkopts"] = CRIS_LINKOPTS + attrs["linkopts"]
    else:
        attrs["linkopts"] = CRIS_LINKOPTS

    return attrs

def cris_cc_library(**attrs):
    native.cc_library(**__add_opts(attrs))

def cris_cc_binary(**attrs):
    native.cc_binary(**__add_opts(attrs))

def cris_cc_test(**attrs):
    if "linkstatic" not in attrs:
        # Mac OS must use static link.
        # See https://github.com/cyfitech/cris-core/issues/239
        attrs["linkstatic"] = select({
            "@platforms//os:macos": True,
            "//conditions:default": False,
        })
    native.cc_test(**__add_opts(attrs))
