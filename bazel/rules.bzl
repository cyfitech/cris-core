CRIS_CXXOPTS = [
    "-Wall",
    "-Wconversion",
    "-Werror",
    "-Wextra",
    "-Wshadow",
    "-g3",
    "-fno-sanitize-recover=all",
]

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

    if "linkstatic" not in attrs:
        attrs["linkstatic"] = True

    return attrs

def cris_cc_library(**attrs):
    native.cc_library(**__add_opts(attrs))

def cris_cc_binary(**attrs):
    native.cc_binary(**__add_opts(attrs))

def cris_cc_test(**attrs):
    native.cc_test(**__add_opts(attrs))
