#include "recorder_config.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <type_traits>

using namespace std;

namespace cris::core {
template<class T>
static void Fail(const T&, const string& info, simdjson::error_code ec) {
    RAW_LOG_FATAL(
        "%s: Failed to parse %s object. %s %s",
        __func__,
        core::GetTypeName<remove_cvref_t<T>>().c_str(),
        info.c_str(),
        simdjson::simdjson_error(ec).what());
}

void ConfigDataParser(RecorderConfig& config, simdjson::ondemand::value& val) {
    simdjson::ondemand::object obj;
    if (const auto ec = val.get(obj)) {
        Fail(config, "JSON object required.", ec);
    }

    string_view record_dir_str;
    if (const auto ec = obj["record_dir"].get(record_dir_str)) {
        if (simdjson::simdjson_error(ec).error() != simdjson::NO_SUCH_FIELD) {
            Fail(config, "Expect a string for \"record_dir\".", ec);
        }
    }
    config.record_dir_ = string(record_dir_str.data(), record_dir_str.size());

    simdjson::ondemand::array array_intervals;
    if (const auto ec = obj["snapshot_intervals"].get(array_intervals)) {
        if (simdjson::simdjson_error(ec).error() == simdjson::NO_SUCH_FIELD) {
            return;
        }
        Fail(config, "Expect a list of objects for \"snapshot_intervals\".", ec);
    }

    for (auto&& data : array_intervals) {
        string_view interval_name;
        if (const auto ec = data["interval_name"].get(interval_name)) {
            Fail(config, "\"interval_name\" is required.", ec);
        }

        int64_t interval_sec = 0;
        if (const auto ec = data["interval_sec"].get(interval_sec)) {
            Fail(config, "\"interval_sec\" is required.", ec);
        }

        config.snapshot_intervals_.push_back(RecorderConfig::IntervalConfig{
            .name_         = string(interval_name.data(), interval_name.size()),
            .interval_sec_ = std::chrono::seconds(interval_sec),
        });
    }
}

}  // namespace cris::core
