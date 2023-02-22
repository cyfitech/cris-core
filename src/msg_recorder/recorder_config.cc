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
    CRIS_CONF_JSON_ENTRY_NORETURN(record_dir, obj, config, "Expect a string for \"record_dir\".");
    config.record_dir_ = string(record_dir.data(), record_dir.size());

    simdjson::ondemand::array snapshot_intervals;
    CRIS_CONF_JSON_ENTRY_RETURN(snapshot_intervals, obj, config, "Expect a list of objects for \"snapshot_intervals\"");

    for (auto&& data : snapshot_intervals) {
        string_view name;
        CRIS_CONF_JSON_ENTRY(name, data, config);

        uint64_t period_sec = 0;
        CRIS_CONF_JSON_ENTRY(period_sec, data, config);

        uint64_t max_num_of_copies = RecorderConfig::IntervalConfig::kDefaultMaxNumOfCopies;
        CRIS_CONF_JSON_ENTRY_NORETURN(
            max_num_of_copies,
            data,
            config,
            "\"max_num_of_copies\" must be an unsigned integer.");

        config.snapshot_intervals_.push_back(RecorderConfig::IntervalConfig{
            .name_              = string(name.data(), name.size()),
            .period_            = std::chrono::seconds(period_sec),
            .max_num_of_copies_ = static_cast<std::size_t>(max_num_of_copies),
        });
    }
}

}  // namespace cris::core
