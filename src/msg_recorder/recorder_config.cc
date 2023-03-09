#include "recorder_config.h"

#include "cris/core/config/config.h"
#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <string_view>

using namespace std;

namespace cris::core {
static void ParseRollingConfig(RecorderConfig& config, simdjson::ondemand::object& obj);

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

    ParseRollingConfig(config, obj);

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

void ParseRollingConfig(RecorderConfig& config, simdjson::ondemand::object& obj) {
    string_view rolling_sv;
    if (const auto ec = obj["rolling"].get(rolling_sv)) {
        if (simdjson::simdjson_error(ec).error() != simdjson::NO_SUCH_FIELD) {
            FailToParseConfig(config, R"(Expect a string for "rolling".)", ec);
        }
        return;
    }

    static const std::map<std::string_view, RecorderConfig::Rolling> rollings{
        {"day", RecorderConfig::Rolling::kDay},
        {"hour", RecorderConfig::Rolling::kHour},
        {"size", RecorderConfig::Rolling::kSize}};

    const auto itr = rollings.find(rolling_sv);
    RAW_CHECK(itr != rollings.cend(), R"(Expect a string for "rolling" be in ["day", "hour", "size"])");

    const auto rolling = itr->second;
    if (rolling != RecorderConfig::Rolling::kSize) {
        config.rolling_ = rolling;
        return;
    }

    // by size
    std::uint64_t            size_limit_mb{};
    static const std::string error_msg{R"(Expect a non-zero positive integer for "size_limit_mb".)"};
    CRIS_CONF_JSON_ENTRY_WITH_MSG(size_limit_mb, obj, "size_limit_mb", config, error_msg);
    RAW_CHECK(size_limit_mb > 0, error_msg.data());

    config.rolling_       = rolling;
    config.size_limit_mb_ = size_limit_mb;
}

}  // namespace cris::core
