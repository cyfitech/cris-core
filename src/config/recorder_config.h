#pragma once

#include <simdjson.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace cris::core {

struct RecorderConfig {
    struct IntervalConfig {
        static constexpr std::size_t kDefaultMaxNumOfCopies = 48;
        std::string                  name_;
        std::chrono::seconds         period_{0};
        std::size_t                  max_num_of_copies_{kDefaultMaxNumOfCopies};
    };

    std::vector<IntervalConfig> snapshot_intervals_;
    std::filesystem::path       record_dir_;
};

void ConfigDataParser(RecorderConfig& config, simdjson::ondemand::value& val);

}  // namespace cris::core
