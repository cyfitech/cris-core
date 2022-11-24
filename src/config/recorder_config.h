#pragma once

#include <simdjson.h>

#include <chrono>
#include <memory>
#include <string>

namespace cris::core {

struct RecorderConfig {
    std::vector<std::chrono::duration<int>> snapshot_intervals_;
    std::string                             record_dir_;
    bool                                    enable_snapshot_{false};
};

using RecorderConfigPtr = std::shared_ptr<RecorderConfig>;

void ConfigDataParser(RecorderConfigPtr& config, simdjson::ondemand::value& val);

}  // namespace cris::core
