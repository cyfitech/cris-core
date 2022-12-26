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

namespace cris::core {

// To read configuration from file.
//
// ConfigFile config_file("my_config.json");
// auto       my_value = config_file.Get<my_value_t>("my_key")->GetValue();
//
// or,
//
// auto my_value = *config_file.Get<my_value_t>("my_key", default_value)->GetValue();

class ConfigFile;

class ConfigBase {
   public:
    ConfigBase() = default;

    ConfigBase(const ConfigBase&) = delete;

    ConfigBase(ConfigBase&&) = delete;

    ConfigBase& operator=(const ConfigBase&) = delete;

    ConfigBase& operator=(ConfigBase&&) = delete;

    virtual ~ConfigBase() = default;

    virtual std::string GetName() const = 0;

    virtual std::string GetTypeName() const = 0;

   protected:
    virtual void InitValue(simdjson::ondemand::value& val) = 0;

    friend class ConfigFile;
};

template<class data_t>
concept ConfigDataType = std::same_as<std::remove_cvref_t<data_t>, data_t>;

template<ConfigDataType data_t>
class Config : public ConfigBase {
   public:
    std::string GetName() const override { return name_; }

    std::string GetTypeName() const override { return core::GetTypeName<data_t>(); }

    const data_t& GetValue() const { return data_; }

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

    template<ConfigDataType data_t>
    std::shared_ptr<Config<data_t>> Get(const std::string& config_name);

    template<ConfigDataType data_t, class arg_t>
    std::shared_ptr<Config<data_t>> Get(const std::string& config_name, arg_t&& default_value);

   protected:
    using config_base_ptr_t = std::shared_ptr<ConfigBase>;
    using config_map_t      = std::unordered_map<std::string, config_base_ptr_t>;

    struct JsonParsingContext {
        simdjson::ondemand::parser   parser_;
        simdjson::padded_string      buf_;
        simdjson::ondemand::document doc_;
    };

    std::shared_ptr<ConfigBase> RegisterOrGet(const std::string& config_name, std::shared_ptr<ConfigBase>&& config);

    void InitConfigJsonContext();

    std::string        filepath_;
    JsonParsingContext json_context_;
    std::mutex         map_mutex_;
    config_map_t       config_map_;
};

template<ConfigDataType data_t>
Config<data_t>::Config(const std::string& name) : name_(name) {
}

namespace impl {

template<ConfigDataType data_t>
std::string ConfigDataGetStringRep(const data_t&) {
    return core::GetTypeName<data_t>();
}

template<>
inline std::string ConfigDataGetStringRep(const std::string& data) {
    // Ignoring escaping for simplicity.
    return "\"" + data + "\"";
}

template<ConfigDataType data_t>
    requires requires(data_t data) { std::to_string(data); }
std::string ConfigDataGetStringRep(const data_t& data) {
    return std::to_string(data);
}

template<ConfigDataType data_t>
    requires requires(data_t data) { std::string(data.Str()); }
std::string ConfigDataGetStringRep(const data_t& data) {
    return data.Str();
}

template<ConfigDataType data_t>
    requires requires(data_t data) { std::string(data->Str()); }
std::string ConfigDataGetStringRep(const data_t& data) {
    return data->Str();
}

template<ConfigDataType T1, ConfigDataType T2>
std::string ConfigDataGetStringRep(const std::pair<T1, T2>& data) {
    return "[" + ConfigDataGetStringRep(data.first) + ", " + ConfigDataGetStringRep(data.second) + "]";
}

template<ConfigDataType data_t>
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

template<ConfigDataType data_t>
std::shared_ptr<Config<data_t>> ConfigFile::Get(const std::string& config_name) {
    return Get<data_t>(config_name, data_t{});
}

template<ConfigDataType data_t, class arg_t>
std::shared_ptr<Config<data_t>> ConfigFile::Get(const std::string& config_name, arg_t&& default_value) {
    static_assert(std::is_same_v<data_t, std::remove_cvref_t<arg_t>>);
    std::shared_ptr<ConfigBase> new_config_ptr(new Config<data_t>(config_name));

    auto config_base_ptr = RegisterOrGet(config_name, std::move(new_config_ptr));
    auto config_ptr      = std::dynamic_pointer_cast<Config<data_t>>(config_base_ptr);
    if (!config_ptr) {
        LOG(FATAL) << __func__ << ": Type mismatched. Config \"" << config_name << "\" is registered as \""
                   << config_base_ptr->GetTypeName() << "\" instead of \"" << GetTypeName<data_t>() << "\".";
    }

    if (!config_ptr->initialized_.load()) {
        LOG(INFO) << __func__ << ": Cannot find Config " << config_name << " in the config file. "
                  << "Use the default value " << impl::ConfigDataGetStringRep(default_value) << " instead.";
        config_ptr->data_ = std::forward<arg_t>(default_value);
    }

    return config_ptr;
}

// Using macro for defining multiple functions.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-errors)
#define __CRIS_CORE_EXTERN_CONFIG_TYPE(type)                                                         \
    /* `type` is a typename so that it cannot be in parentheses.      */                             \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses,-warnings-as-errors) */                             \
    extern template class Config<type>;                                                              \
                                                                                                     \
    /* `type` is a typename so that it cannot be in parentheses.      */                             \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses,-warnings-as-errors) */                             \
    extern template std::shared_ptr<Config<type>> ConfigFile::Get<type>(const std::string&);         \
                                                                                                     \
    /* `type` is a typename so that it cannot be in parentheses.      */                             \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses,-warnings-as-errors) */                             \
    extern template std::shared_ptr<Config<type>> ConfigFile::Get<type>(const std::string&, type&);  \
                                                                                                     \
    /* `type` is a typename so that it cannot be in parentheses.      */                             \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses,-warnings-as-errors) */                             \
    extern template std::shared_ptr<Config<type>> ConfigFile::Get<type>(const std::string&, type&&); \
                                                                                                     \
    namespace impl {                                                                                 \
    /* `type` is a typename so that it cannot be in parentheses.      */                             \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses,-warnings-as-errors) */                             \
    extern template std::string ConfigDataGetStringRep(const type&);                                 \
    }

__CRIS_CORE_EXTERN_CONFIG_TYPE(bool)
__CRIS_CORE_EXTERN_CONFIG_TYPE(int)
__CRIS_CORE_EXTERN_CONFIG_TYPE(unsigned)
__CRIS_CORE_EXTERN_CONFIG_TYPE(long)
__CRIS_CORE_EXTERN_CONFIG_TYPE(unsigned long)
__CRIS_CORE_EXTERN_CONFIG_TYPE(long long)
__CRIS_CORE_EXTERN_CONFIG_TYPE(unsigned long long)
__CRIS_CORE_EXTERN_CONFIG_TYPE(float)
__CRIS_CORE_EXTERN_CONFIG_TYPE(double)

#undef __CRIS_CORE_EXTERN_CONFIG_TYPE

template<ConfigDataType data_t>
void Config<data_t>::InitValue(simdjson::ondemand::value& val) {
    if constexpr (std::is_integral_v<data_t>) {
        if constexpr (std::is_same_v<data_t, bool>) {
            data_ = ConfigDataTrivialParser<bool>(val);
        } else if constexpr (!std::is_signed_v<data_t>) {
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

    LOG(INFO) << __func__ << ": Config \"" << GetName() << "\" is set to " << impl::ConfigDataGetStringRep(data_)
              << ".";
}

}  // namespace cris::core
