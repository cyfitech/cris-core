#pragma once

#include <simdjson.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

namespace cris::core {

struct RecorderConfig {
    std::vector<std::chrono::seconds> snapshot_intervals_;
    std::string                       interval_name_;
    std::filesystem::path             record_dir_;
};

using RecorderConfigPtr = std::shared_ptr<RecorderConfig>;

void ConfigDataParser(RecorderConfigPtr& config, simdjson::ondemand::value& val);

}  // namespace cris::core
