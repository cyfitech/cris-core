#include "cris/core/config/config.h"

#include "cris/core/utils/logging.h"

#include <simdjson.h>

#include <memory>
#include <mutex>
#include <utility>

namespace cris::core {

ConfigFile::ConfigFile(const std::string& filepath) : filepath_(filepath) {
    InitConfigJsonContext();
}

ConfigBase* ConfigFile::RegisterOrGet(const std::string& config_name, std::unique_ptr<ConfigBase>&& config) {
    std::lock_guard<std::mutex> lock(map_mutex_);

    auto config_search = config_map_.find(config_name);
    if (config_search != config_map_.end()) {
        return config_search->second.get();
    }

    simdjson::ondemand::value val;
    if (json_context_.doc_[config_name].get(val) == simdjson::error_code::SUCCESS) {
        config->InitValue(val);
        return config_map_.emplace(config_name, std::move(config)).first->second.get();
    } else {
        return tmp_config_list_.emplace_back(std::move(config)).get();
    }
}

void ConfigFile::InitConfigJsonContext() {
    if (const auto load_err = simdjson::padded_string::load(filepath_).get(json_context_.buf_)) {
        LOG(WARNING) << __func__ << ": Failed to load Config:" << filepath_ << " "
                     << simdjson::simdjson_error(load_err).what();
        return;
    }

    if (const auto parse_err = json_context_.parser_.iterate(json_context_.buf_).get(json_context_.doc_)) {
        LOG(WARNING) << __func__ << ": Failed to parse Config:" << filepath_ << " "
                     << simdjson::simdjson_error(parse_err).what() << json_context_.buf_;
        return;
    }
}

}  // namespace cris::core
