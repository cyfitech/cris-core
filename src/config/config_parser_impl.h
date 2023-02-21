#pragma once

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include <simdjson.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <type_traits>

namespace cris::core {

template<class data_t>
inline void ConfigDataParser(data_t& /* data */, simdjson::ondemand::value& /* val */) {
    static_assert(!std::is_same_v<data_t, data_t>, "Unimplemented data parser.");
    // Never reach here.
    std::abort();
}

template<class data_t>
inline data_t ConfigDataTrivialParser(simdjson::ondemand::value& val) {
    data_t     res{};
    const auto error = val.get(res);
    if (error) [[unlikely]] {
        LOG(FATAL) << __func__ << ": Failed to parse JSON value: " << simdjson::simdjson_error(error).what();
    }
    return res;
}

template<class T>
[[noreturn]] static void Fail(const T&, const std::string& info, simdjson::error_code ec) {
    RAW_LOG_FATAL(
        "%s: Failed to parse %s object. %s %s",
        __func__,
        core::GetTypeName<std::remove_cvref_t<T>>().c_str(),
        info.c_str(),
        simdjson::simdjson_error(ec).what());
    abort();
}

#define CRIS_CONFIG_PARSE_IMP(config, obj, name, val, is_expert, is_return, extra_message) \
    if (const auto ec = obj[name].get(val)) {                                        \
        if (is_expert) {                                                             \
            if (simdjson::simdjson_error(ec).error() != simdjson::NO_SUCH_FIELD) {   \
                Fail(config, extra_message, ec);                                     \
            }                                                                        \
        } else {                                                                     \
            Fail(config, extra_message, ec);                                         \
        }                                                                            \
        if (is_return) {                                                             \
            return;                                                                  \
        }                                                                            \
    }

#define CRIS_CONFIG_PARSE(config, obj, name, val, extra_message) \
    CRIS_CONFIG_PARSE_IMP(config, obj, name, val, false, false, extra_message)

#define CRIS_CONFIG_EXPERT_PARSE_NORETURN(config, obj, name, val, extra_message) \
    CRIS_CONFIG_PARSE_IMP(config, obj, name, val, true, false, extra_message)

#define CRIS_CONFIG_EXPERT_PARSE_RETURN(config, obj, name, val, extra_message) \
    CRIS_CONFIG_PARSE_IMP(config, obj, name, val, true, true, extra_message)
}  // namespace cris::core
