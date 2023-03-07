#pragma once

#include <simdjson.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace cris::core {

struct RecorderConfig {
    enum class Rolling {
        NONE = 0,
        DAY  = 1,
        HOUR = 2,
        SIZE = 3,
    };

    struct IntervalConfig {
        std::string          name_;
        std::chrono::seconds period_;
        std::size_t          max_num_of_copies_{kDefaultMaxNumOfCopies};

        static constexpr std::size_t kDefaultMaxNumOfCopies = 48;
    };

    std::vector<IntervalConfig> snapshot_intervals_;
    std::filesystem::path       record_dir_;
    Rolling                     rolling_{Rolling::NONE};
    std::uint64_t               size_limit_mb_{std::numeric_limits<std::uint64_t>::max()};
};

void ConfigDataParser(RecorderConfig& config, simdjson::ondemand::value& val);

}  // namespace cris::core
