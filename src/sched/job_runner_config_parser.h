#pragma once

#include "cris/core/sched/job_runner.h"

#include <simdjson.h>

namespace cris::core {

void ConfigDataParser(JobRunner::Config& config, simdjson::ondemand::value& val);

}  // namespace cris::core
