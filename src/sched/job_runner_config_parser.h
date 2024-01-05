#pragma once

#include "cris/core/sched/job_runner.h"
#include "cris/core/config/config.h"

#include <simdjson.h>

namespace cris::core {

void ConfigDataParser(JobRunner::Config& config, simdjson::ondemand::value& val);

JobRunner::Config GetCoreRunConfig(ConfigFile& runner_config_file);
JobRunner::Config GetPeripheralRunConfig(ConfigFile& runner_config_file);

}  // namespace cris::core
