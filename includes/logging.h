#pragma once

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <glog/logging.h>
#include <glog/raw_logging.h>
#include <glog/stl_logging.h>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
