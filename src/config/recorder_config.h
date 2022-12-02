#pragma once

#include <simdjson.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace cris::core {

struct RecorderConfig {
    struct IntervalConfig {
        std::string          name_;
        std::chrono::seconds interval_sec_;
    };

    std::vector<IntervalConfig> snapshot_intervals_;
    std::filesystem::path       record_dir_;
};

void ConfigDataParser(RecorderConfig& config, simdjson::ondemand::value& val);

}  // namespace cris::core
