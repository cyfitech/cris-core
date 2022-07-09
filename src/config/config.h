#pragma once

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include "config_parser_impl.h"

#include <simdjson.h>

#include <atomic>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cris::core {

// To read configuration from file.
//
// ConfigFile config_file("my_config.json");
// auto       my_value = *config_file.Get<my_value_t>("my_key");
//
// or,
//
// auto my_value = *config_file.Get<my_value_t>("my_key", default_value);

class ConfigFile;

class ConfigBase {
   public:
    ConfigBase() = default;

    ConfigBase(const ConfigBase&) = delete;

    ConfigBase(ConfigBase&&) = delete;

    ConfigBase& operator=(const ConfigBase&) = delete;

    virtual ~ConfigBase() = default;

    virtual std::string GetName() const = 0;

   protected:
    virtual void InitValue(simdjson::ondemand::value& val) = 0;

    friend class ConfigFile;
};

template<class data_t>
class Config : public ConfigBase {
   public:
    const data_t& operator*() const { return data_; }

    const data_t* operator->() const { return &**this; }

    std::string GetName() const override { return name_; }

   protected:
    explicit Config(const std::string& name);

    void InitValue(simdjson::ondemand::value& val) override;

    friend class ConfigFile;

    const std::string name_;
    std::atomic<bool> initialized_{false};
    data_t            data_{};
};

class ConfigFile {
   public:
    explicit ConfigFile(const std::string& filepath);

    template<class data_t>
    const Config<data_t>& Get(const std::string& config_name);

    template<class data_t>
    const Config<data_t>& Get(const std::string& config_name, data_t&& default_value);

   private:
    using config_base_ptr_t = std::unique_ptr<ConfigBase>;
    using config_map_t      = std::unordered_map<std::string, config_base_ptr_t>;
    using tmp_config_list_t = std::vector<config_base_ptr_t>;

    struct JsonParsingContext {
        simdjson::ondemand::parser   parser_;
        simdjson::padded_string      buf_;
        simdjson::ondemand::document doc_;
    };

    ConfigBase* RegisterOrGet(const std::string& config_name, std::unique_ptr<ConfigBase>&& config);

    void InitConfigJsonContext();

    std::string        filepath_;
    JsonParsingContext json_context_;
    std::mutex         map_mutex_;
    config_map_t       config_map_;
    tmp_config_list_t  tmp_config_list_;
};

template<class data_t>
Config<data_t>::Config(const std::string& name) : name_(name) {
}

namespace impl {

template<class data_t>
std::string ConfigDataGetStringRep(const data_t&) {
    return core::GetTypeName<data_t>();
}

template<>
inline std::string ConfigDataGetStringRep(const std::string& data) {
    // Ignoring escaping for simplicity.
    return "\"" + data + "\"";
}

template<class data_t>
    requires requires(data_t data) { std::to_string(data); }
std::string ConfigDataGetStringRep(const data_t& data) {
    return std::to_string(data);
}

template<class data_t>
    requires requires(data_t data) { std::string(data.Str()); }
std::string ConfigDataGetStringRep(const data_t& data) {
    return data.Str();
}

template<class data_t>
    requires requires(data_t data) { std::string(data->Str()); }
std::string ConfigDataGetStringRep(const data_t& data) {
    return data->Str();
}

template<class T1, class T2>
std::string ConfigDataGetStringRep(const std::pair<T1, T2>& data) {
    return "[" + ConfigDataGetStringRep(data.first) + ", " + ConfigDataGetStringRep(data.second) + "]";
}

template<class data_t>
    // clang-format off
    requires(!std::same_as<data_t, std::string>) && requires(data_t data) {
        std::accumulate(
            data.begin(),
            data.end(),
            std::declval<std::string>(),
            std::declval<std::function<std::string(std::string, const typename data_t::value_type&)>>());
    }
// clang-format on
std::string ConfigDataGetStringRep(const data_t& data) {
    return std::accumulate(
               data.begin(),
               data.end(),
               std::string(),
               [](std::string result, const auto& item) {
                   result += result.empty() ? "[" : ",";
                   return std::move(result) + ConfigDataGetStringRep(item);
               }) +
        "]";
}

}  // namespace impl

template<class data_t>
const Config<data_t>& ConfigFile::Get(const std::string& config_name) {
    return Get(config_name, data_t{});
}

template<class data_t>
const Config<data_t>& ConfigFile::Get(const std::string& config_name, data_t&& default_value) {
    std::unique_ptr<ConfigBase> new_config_ptr(new Config<data_t>(config_name));

    auto* config_base_ptr = RegisterOrGet(config_name, std::move(new_config_ptr));
    auto* config_ptr      = dynamic_cast<Config<data_t>*>(config_base_ptr);
    if (!config_ptr) {
        LOG(FATAL) << __func__ << ": Config " << config_name << " is registered, but not " << GetTypeName<data_t>();
    }

    if (!config_ptr->initialized_.load()) {
        LOG(INFO) << __func__ << ": Cannot find Config " << config_name << " in the config file. "
                  << "Use the default value " << impl::ConfigDataGetStringRep(default_value) << " instead.";
        config_ptr->data_ = std::move(default_value);
    }

    return *config_ptr;
}

template<class data_t>
void Config<data_t>::InitValue(simdjson::ondemand::value& val) {
    if constexpr (std::is_integral_v<data_t>) {
        if constexpr (!std::is_signed_v<data_t>) {
            // signed int
            data_ = static_cast<data_t>(ConfigDataTrivialParser<std::int64_t>(val));
        } else {
            // unsigned int
            data_ = static_cast<data_t>(ConfigDataTrivialParser<std::uint64_t>(val));
        }
    } else if constexpr (std::is_floating_point_v<data_t>) {
        // floating point
        data_ = static_cast<data_t>(ConfigDataTrivialParser<double>(val));
    } else {
        // others
        ConfigDataParser(data_, val);
    }
    initialized_.store(true);

    LOG(INFO) << __func__ << ": Config " << GetName() << " is set to " << impl::ConfigDataGetStringRep(data_);
}

}  // namespace cris::core
