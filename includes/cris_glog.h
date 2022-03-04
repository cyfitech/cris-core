#pragma once

#if defined(__clang__)
#define CRIS_COMPILER clang
#elif defined(__GNUG__)
#define CRIS_COMPILER GCC
#elif
#error Only support gcc or clang compiler!
#endif

#pragma CRIS_COMPILER diagnostic push
#pragma CRIS_COMPILER diagnostic ignored "-Wconversion"
#include <glog/logging.h>
#pragma CRIS_COMPILER diagnostic pop

#undef CRIS_COMPILER
