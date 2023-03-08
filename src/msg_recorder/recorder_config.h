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
        kNone = 0,
        kDay  = 1,
        kHour = 2,
        kSize = 3,
    };

    struct IntervalConfig {
        std::string          name_;
        std::chrono::seconds period_;
        std::size_t          max_num_of_copies_{kDefaultMaxNumOfCopies};

        static constexpr std::size_t kDefaultMaxNumOfCopies = 48;
    };

    std::vector<IntervalConfig> snapshot_intervals_;
    std::filesystem::path       record_dir_;
    std::string                 hostname_;
    Rolling                     rolling_{Rolling::kNone};
    std::uint64_t               size_limit_mb_{std::numeric_limits<std::uint64_t>::max()};
};

void ConfigDataParser(RecorderConfig& config, simdjson::ondemand::value& val);

}  // namespace cris::core
