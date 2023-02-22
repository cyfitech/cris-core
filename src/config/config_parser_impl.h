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

}  // namespace cris::core
