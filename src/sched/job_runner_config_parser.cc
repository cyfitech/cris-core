#include "cris/core/sched/job_runner_config_parser.h"

#include "cris/core/utils/logging.h"

#include <simdjson.h>

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace cris::core {

void ConfigDataParser(JobRunner::Config& config, simdjson::ondemand::value& val) {
    simdjson::ondemand::object obj;
    if (val.get_object().get(obj)) {
        RAW_LOG_FATAL("%s: Failed to get Strategy JSON object. Check your configuration file.", __func__);
    }

    config = JobRunner::Config();

    {
        std::uint64_t thread_num = 0;
        if (obj["thread_num"].get(thread_num) == simdjson::error_code::SUCCESS) {
            config.thread_num_ = static_cast<std::size_t>(thread_num);
        }
    }

    {
        std::uint64_t always_active_thread_num = 0;
        if (obj["always_active"].get(always_active_thread_num) == simdjson::error_code::SUCCESS) {
            config.always_active_thread_num_ = static_cast<std::size_t>(always_active_thread_num);
        }
    }

    {
        std::uint64_t active_ms = 0;
        if (obj["active_ms"].get(active_ms) == simdjson::error_code::SUCCESS) {
            config.active_time_ = std::chrono::milliseconds(active_ms);
        }
    }
}

}  // namespace cris::core
