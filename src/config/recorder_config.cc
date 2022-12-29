#include "recorder_config.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

using namespace std;

namespace cris::core {
static void Fail(const char* info, simdjson::error_code ec) {
    RAW_LOG_FATAL(
        "%s: Failed to parse cris::core::RecorderConfig object. %s %s",
        __func__,
        info,
        simdjson::simdjson_error(ec).what());
}

void ConfigDataParser(RecorderConfig& config, simdjson::ondemand::value& val) {
    simdjson::ondemand::object obj;
    if (const auto ec = val.get(obj)) {
        Fail("JSON object required.", ec);
    }

    string_view record_dir_str;
    if (const auto ec = obj["record_dir"].get(record_dir_str)) {
        if (simdjson::simdjson_error(ec).error() != simdjson::NO_SUCH_FIELD) {
            Fail("Expect a string for \"record_dir\".", ec);
        }
    } else {
        config.record_dir_ = string(record_dir_str.data(), record_dir_str.size());
    }

    simdjson::ondemand::array array_intervals;
    if (const auto ec = obj["snapshot_intervals"].get(array_intervals)) {
        if (simdjson::simdjson_error(ec).error() == simdjson::NO_SUCH_FIELD) {
            return;
        }
        Fail("Expect a list of objects for \"snapshot_intervals\".", ec);
    }

    for (auto&& data : array_intervals) {
        string_view name;
        if (const auto ec = data["name"].get(name)) {
            Fail("\"name\" is required.", ec);
        }

        int64_t period_sec = 0;
        if (const auto ec = data["period_sec"].get(period_sec)) {
            Fail("\"period_sec\" is required.", ec);
        }

        std::size_t max_num_of_copies = 0;
        if (const auto ec = data["max_num_of_copies"].get(max_num_of_copies)) {
            if (simdjson::simdjson_error(ec).error() != simdjson::NO_SUCH_FIELD) {
                Fail("Expect a number for \"max_num_of_copies\".", ec);
            }
        }

        config.snapshot_intervals_.push_back(RecorderConfig::IntervalConfig{
            .name_         = string(name.data(), name.size()),
            .interval_sec_ = std::chrono::seconds(period_sec),
        });

        if (max_num_of_copies != 0) {
            config.snapshot_intervals_.back().max_copy_ = max_num_of_copies;
        }
    }
}

}  // namespace cris::core
