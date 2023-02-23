#include "recorder_config.h"

#include "cris/core/config/config.h"
#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

using namespace std;

namespace cris::core {
void ConfigDataParser(RecorderConfig& config, simdjson::ondemand::value& val) {
    simdjson::ondemand::object obj;
    if (const auto ec = val.get(obj)) {
        FailToParseConfig(config, "JSON object required.", ec);
    }

    string_view record_dir;
    if (const auto ec = obj["record_dir"].get(record_dir)) {
        if (simdjson::simdjson_error(ec).error() != simdjson::NO_SUCH_FIELD) {
            FailToParseConfig(config, "Expect a string for \"record_dir\".", ec);
        }
    } else {
        config.record_dir_ = string(record_dir.data(), record_dir.size());
    }

    simdjson::ondemand::array snapshot_intervals;
    if (const auto ec = obj["snapshot_intervals"].get(snapshot_intervals)) {
        if (simdjson::simdjson_error(ec).error() == simdjson::NO_SUCH_FIELD) {
            return;
        }
        FailToParseConfig(config, "Expect a list of objects for \"snapshot_intervals\".", ec);
    }

    for (auto&& data : snapshot_intervals) {
        string_view name;
        CRIS_CONF_JSON_ENTRY(name, data, "name", config);

        uint64_t period_sec = 0;
        CRIS_CONF_JSON_ENTRY(period_sec, data, "period_sec", config);

        uint64_t max_num_of_copies = RecorderConfig::IntervalConfig::kDefaultMaxNumOfCopies;
        if (const auto ec = data["max_num_of_copies"].get(max_num_of_copies)) {
            if (simdjson::simdjson_error(ec).error() != simdjson::NO_SUCH_FIELD) {
                FailToParseConfig(config, "\"max_num_of_copies\" must be an unsigned integer.", ec);
            }
        }

        config.snapshot_intervals_.push_back(RecorderConfig::IntervalConfig{
            .name_              = string(name.data(), name.size()),
            .period_            = std::chrono::seconds(period_sec),
            .max_num_of_copies_ = static_cast<std::size_t>(max_num_of_copies),
        });
    }
}

}  // namespace cris::core
