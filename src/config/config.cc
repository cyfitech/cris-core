#include "cris/core/config/config.h"

#include "cris/core/utils/logging.h"

#include <simdjson.h>

#include <memory>
#include <mutex>
#include <utility>

namespace cris::core {

#define __CRIS_CORE_DEFINE_CONFIG_TYPE(type)                                                  \
    template class Config<type>;                                                              \
                                                                                              \
    template std::shared_ptr<Config<type>> ConfigFile::Get<type>(const std::string&);         \
                                                                                              \
    template std::shared_ptr<Config<type>> ConfigFile::Get<type>(const std::string&, type&);  \
                                                                                              \
    template std::shared_ptr<Config<type>> ConfigFile::Get<type>(const std::string&, type&&); \
                                                                                              \
    namespace impl {                                                                          \
    template std::string ConfigDataGetStringRep(const type&);                                 \
    }

__CRIS_CORE_DEFINE_CONFIG_TYPE(bool)
__CRIS_CORE_DEFINE_CONFIG_TYPE(int)
__CRIS_CORE_DEFINE_CONFIG_TYPE(unsigned)
__CRIS_CORE_DEFINE_CONFIG_TYPE(long)
__CRIS_CORE_DEFINE_CONFIG_TYPE(unsigned long)
__CRIS_CORE_DEFINE_CONFIG_TYPE(long long)
__CRIS_CORE_DEFINE_CONFIG_TYPE(unsigned long long)
__CRIS_CORE_DEFINE_CONFIG_TYPE(float)
__CRIS_CORE_DEFINE_CONFIG_TYPE(double)

#undef __CRIS_CORE_DEFINE_CONFIG_TYPE

ConfigFile::ConfigFile(const std::string& filepath) : filepath_(filepath) {
    InitConfigJsonContext();
}

std::shared_ptr<ConfigBase> ConfigFile::RegisterOrGet(
    const std::string&            config_name,
    std::shared_ptr<ConfigBase>&& config) {
    std::lock_guard<std::mutex> lock(map_mutex_);

    auto config_search = config_map_.find(config_name);
    if (config_search != config_map_.end()) {
        return config_search->second;
    }

    simdjson::ondemand::value val;
    if (json_context_.doc_[config_name].get(val) == simdjson::error_code::SUCCESS) {
        config->InitValue(val);
        return config_map_.emplace(config_name, std::move(config)).first->second;
    }

    return std::move(config);
}

void ConfigFile::InitConfigJsonContext() {
    if (const auto load_err = simdjson::padded_string::load(filepath_).get(json_context_.buf_)) {
        LOG(WARNING) << __func__ << ": Failed to load Config: " << filepath_ << " "
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
