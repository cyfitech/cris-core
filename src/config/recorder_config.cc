#include "recorder_config.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include <simdjson.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>
#include <type_traits>

using namespace std;

namespace cris::core {
template<class T>
[[noreturn]] static void Fail(const T&, const string& info, simdjson::error_code ec) {
    RAW_LOG_FATAL(
        "%s: Failed to parse %s object. %s %s",
        __func__,
        core::GetTypeName<remove_cvref_t<T>>().c_str(),
        info.c_str(),
        simdjson::simdjson_error(ec).what());
    abort();
}

void ConfigDataParser(RecorderConfigPtr& config, simdjson::ondemand::value& val) {
    config = std::make_shared<RecorderConfig>();

    simdjson::ondemand::object obj;
    if (const auto ec = val.get(obj)) {
        Fail(config, "JSON object required.", ec);
    }

    simdjson::ondemand::array array_intervals;
    if (const auto ec = obj["snapshot_intervals_sec"].get(array_intervals)) {
        Fail(config, "\"snapshot_intervals_\" is required.", ec);
    }

    for (auto&& data : array_intervals) {
        int64_t each_interval = 0;
        if (const auto ec = data.get(each_interval)) {
            Fail(config, "\"intervals\" is required.", ec);
        }
        config->snapshot_intervals_.push_back(std::chrono::duration<int>(each_interval));
    }

    string_view record_dir_str;
    if (const auto ec = obj["record_dir"].get(record_dir_str)) {
        Fail(config, "\"record_dir\" is required.", ec);
    }

    if (const auto ec = obj["enable_snapshot"].get(config->enable_snapshot_)) {
        Fail(config, "\"enable_snapshot\" is required.", ec);
    }

    config->record_dir_ = string(record_dir_str.data(), record_dir_str.size());
}

}  // namespace cris::core
