#pragma once

#include <simdjson.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace cris::core {

// Save 48 snapshots locally for each time interval as default
constexpr std::size_t kDefaultMaxCopyNum = 48;

struct RecorderConfig {
    struct IntervalConfig {
        std::string          name_;
        std::chrono::seconds interval_sec_;
        std::size_t          max_copy_{kDefaultMaxCopyNum};
    };

    std::vector<IntervalConfig> snapshot_intervals_;
    std::filesystem::path       record_dir_;
};

void ConfigDataParser(RecorderConfig& config, simdjson::ondemand::value& val);

}  // namespace cris::core
