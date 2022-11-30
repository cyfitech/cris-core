#include "recorder_config.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include <simdjson.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <iostream>
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

    simdjson::ondemand::array array_intervals;
    if (const auto ec = obj["snapshot_intervals_sec"].get(array_intervals)) {
        Fail(config, "\"snapshot_intervals_sec\" is required.", ec);
    }

    for (auto&& data : array_intervals) {
        int64_t each_interval_sec = 0;
        if (const auto ec = data.get(each_interval_sec)) {
            Fail(config, "an actual interval in seconds is required.", ec);
        }
        config.snapshot_intervals_.push_back(
            RecorderConfig::IntervalConfig{
                .dir_name_ = GenrerateDirName(each_interval_sec),
                .interval_sec_ = std::chrono::seconds(each_interval_sec),
            }
            );
    }

    string_view record_dir_str;
    if (const auto ec = obj["record_dir"].get(record_dir_str)) {
        Fail(config, "\"record_dir\" is required.", ec);
    }
    config.record_dir_ = string(record_dir_str.data(), record_dir_str.size());
}

std::string GenrerateDirName(int64_t interval){
    auto selected = RecorderConfig::DirName::SECONDLY;
    for(auto name : dir_names_){
        if(interval < int64_t(name)){
            break;
        }
        selected = name;
    }
    return to_string(int64_t(selected));
}

}  // namespace cris::core
