CRIS_CXXOPTS = [
    "-Wall",
    "-Wfloat-conversion",
    "-Werror",
    "-Wextra",
    "-g3",
    "-std=c++20",
]

CRIS_LINKOPTS = [
    "-Wall",
    "-Werror",
    "-Wextra",
    "-Wno-fuse-ld-path",
]

def __add_opts(attrs):
    if "copts" in attrs and attrs["copts"] != None:
        attrs["copts"] = attrs["copts"] + CRIS_CXXOPTS
    else:
        attrs["copts"] = CRIS_CXXOPTS

    if "linkopts" in attrs and attrs["linkopts"] != None:
        attrs["linkopts"] = attrs["linkopts"] + CRIS_LINKOPTS
    else:
        attrs["linkopts"] = CRIS_LINKOPTS

    return attrs

def cris_cc_library(**attrs):
    native.cc_library(**__add_opts(attrs))

def cris_cc_binary(**attrs):
    native.cc_binary(**__add_opts(attrs))

def cris_cc_test(**attrs):
    native.cc_test(**__add_opts(attrs))
