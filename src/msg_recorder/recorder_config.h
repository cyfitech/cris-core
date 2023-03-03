#pragma once

#include <simdjson.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace cris::core {

struct RecorderConfig {
    struct IntervalConfig {
        std::string          name_;
        std::chrono::seconds period_;
        std::size_t          max_num_of_copies_{kDefaultMaxNumOfCopies};

        static constexpr std::size_t kDefaultMaxNumOfCopies = 48;
    };

    std::vector<IntervalConfig> snapshot_intervals_;
    std::filesystem::path       record_dir_;
};

struct SnapshotInfo {
    std::filesystem::path snapshot_dir_;
};

void ConfigDataParser(RecorderConfig& config, simdjson::ondemand::value& val);

}  // namespace cris::core
