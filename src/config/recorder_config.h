#pragma once

#include <simdjson.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace cris::core {

struct RecorderConfig {
    enum class DirName : int64_t {
        SECONDLY = 1,
        MINUTELY = 1 * 60,
        HOURLY = 1 * 60 * 60,
        DAILY = 1 * 60 * 60 * 24,
        WEEKLY = 1 * 60 * 60 * 24 * 7,
        MONTHLY = 1 * 60 * 60 * 24 * 7 * 4,
    };

    struct IntervalConfig {
        std::string dir_name_;
        std::chrono::seconds interval_sec_;
    };

    std::vector<IntervalConfig> snapshot_intervals_;
    std::filesystem::path             record_dir_;
};

std::vector<RecorderConfig::DirName> dir_names_ = {RecorderConfig::DirName::SECONDLY, RecorderConfig::DirName::MINUTELY, RecorderConfig::DirName::HOURLY, RecorderConfig::DirName::DAILY, RecorderConfig::DirName::WEEKLY, RecorderConfig::DirName::MONTHLY};

void ConfigDataParser(RecorderConfig& config, simdjson::ondemand::value& val);

std::string GenrerateDirName(int64_t interval);

}  // namespace cris::core
