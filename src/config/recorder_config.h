#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace simdjson::SIMDJSON_BUILTIN_IMPLEMENTATION::ondemand {
class value;
}

namespace cris::core {

struct RecorderConfig {
    struct IntervalConfig {
        std::string          name_;
        std::chrono::seconds interval_sec_;
    };

    std::vector<IntervalConfig> snapshot_intervals_;
    std::filesystem::path       record_dir_;
};

void ConfigDataParser(RecorderConfig& config, simdjson::SIMDJSON_BUILTIN_IMPLEMENTATION::ondemand::value& val);

}  // namespace cris::core
