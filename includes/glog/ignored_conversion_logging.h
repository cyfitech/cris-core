#pragma once

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#if defined(GLOG_LOGGING_IGNORED_CONVERSION)
#include <glog/logging.h>
#endif
#if defined(GLOG_RAW_LOGGING_IGNORED_CONVERSION)
#include <glog/raw_logging.h>
#endif
#if defined(GLOG_STL_LOGGING_IGNORED_CONVERSION)
#include <glog/stl_logging.h>
#endif
#pragma clang diagnostic pop
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#if defined(GLOG_LOGGING_IGNORED_CONVERSION)
#include <glog/logging.h>
#endif
#if defined(GLOG_RAW_LOGGING_IGNORED_CONVERSION)
#include <glog/raw_logging.h>
#endif
#if defined(GLOG_STL_LOGGING_IGNORED_CONVERSION)
#include <glog/stl_logging.h>
#endif
#pragma GCC diagnostic pop
#endif
