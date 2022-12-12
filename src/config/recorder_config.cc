Skip to content
Search or jump to…
Pull requests
Issues
Codespaces
Marketplace
Explore
 
@YuzhouGuo 
cyfitech
/
cris-core
Public
Code
Issues
4
Pull requests
5
Actions
Projects
Wiki
Security
Insights
cris-core/src/config/recorder_config.cc
@YuzhouGuo
YuzhouGuo Add message recorder config file (#106)
…
Latest commit f01ad3e 1 hour ago
 History
 1 contributor
64 lines (53 sloc)  1.96 KB

#include "recorder_config.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

using namespace std;

namespace cris::core {
static void Fail(const char* info, simdjson::error_code ec) {
    RAW_LOG_FATAL(
        "%s: Failed to parse cris::core::RecorderConfig object. %s %s",
        __func__,
        info,
        simdjson::simdjson_error(ec).what());
}

void ConfigDataParser(RecorderConfig& config, simdjson::ondemand::value& val) {
    simdjson::ondemand::object obj;
    if (const auto ec = val.get(obj)) {
        Fail("JSON object required.", ec);
    }

    string_view record_dir_str;
    if (const auto ec = obj["record_dir"].get(record_dir_str)) {
        if (simdjson::simdjson_error(ec).error() != simdjson::NO_SUCH_FIELD) {
            Fail("Expect a string for \"record_dir\".", ec);
        }
    } else {
        config.record_dir_ = string(record_dir_str.data(), record_dir_str.size());
    }

    simdjson::ondemand::array array_intervals;
    if (const auto ec = obj["snapshot_intervals"].get(array_intervals)) {
        if (simdjson::simdjson_error(ec).error() == simdjson::NO_SUCH_FIELD) {
            return;
        }
        Fail("Expect a list of objects for \"snapshot_intervals\".", ec);
    }

    for (auto&& data : array_intervals) {
        string_view interval_name;
        if (const auto ec = data["interval_name"].get(interval_name)) {
            Fail("\"interval_name\" is required.", ec);
        }

        int64_t interval_sec = 0;
        if (const auto ec = data["interval_sec"].get(interval_sec)) {
            Fail("\"interval_sec\" is required.", ec);
        }

        config.snapshot_intervals_.push_back(RecorderConfig::IntervalConfig{
            .name_         = string(interval_name.data(), interval_name.size()),
            .interval_sec_ = std::chrono::seconds(interval_sec),
        });
    }
}

}  // namespace cris::core
Footer
© 2022 GitHub, Inc.
Footer navigation
Terms
Privacy
Security
Status
Docs
Contact GitHub
Pricing
API
Training
Blog
About
