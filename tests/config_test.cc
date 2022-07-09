#include "cris/core/config/config.h"

#include <gtest/gtest.h>

#include <fmt/core.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace cris::core {

constexpr double kEps = 1e-6;

namespace fs = std::filesystem;

class ConfigTest : public testing::Test {
   public:
    ConfigTest()
        : testing::Test()
        , test_config_dir_(std::tmpnam(nullptr))
        , test_config_path_(test_config_dir_ / "test.config") {
        EXPECT_TRUE(fs::create_directory(test_config_dir_));
    }

    ~ConfigTest() { fs::remove_all(test_config_dir_); }

    ConfigFile MakeConfigFile(std::string content) const {
        std::ofstream config_file(test_config_path_);
        config_file << std::move(content);
        config_file.flush();
        return ConfigFile(test_config_path_);
    }

   private:
    fs::path test_config_dir_;
    fs::path test_config_path_;
};

TEST_F(ConfigTest, Configs) {
    const std::string int_config_name     = "int_config";
    int               int_config_value    = 123;
    const std::string double_config_name  = "double_config";
    double            double_config_value = 3.14;

    const std::string int_config_with_default_1_name  = "int_config_wd_1";
    int               int_config_with_default_1_value = 23333;
    const std::string int_config_with_default_2_name  = "int_config_wd_2";
    int               int_config_with_default_2_value = 77777;

    auto config_file = MakeConfigFile(fmt::format(
        R"({{
            "{}": {},
            "{}": {},
            "{}": {}
        }})",
        int_config_name,
        int_config_value,
        double_config_name,
        double_config_value,
        // wd_1 uses the default value, wd_2 uses the value in the config
        int_config_with_default_2_name,
        int_config_with_default_2_value));

    EXPECT_EQ(*config_file.Get<int>(int_config_name), int_config_value);
    EXPECT_NEAR(*config_file.Get<double>(double_config_name), double_config_value, kEps);

    // No value in the config file, so the value passed to Register will be picked up.
    EXPECT_EQ(
        *config_file.Get<int>(int_config_with_default_1_name, int(int_config_with_default_1_value)),
        int_config_with_default_1_value);

    // There is a value in the config file, the value here will be ignored.
    EXPECT_EQ(*config_file.Get<int>(int_config_with_default_2_name, 0), int_config_with_default_2_value);
}

// Create extra namespace to make sure ADL find the Parser correctly
namespace inner {

struct NonCopyableType {
    using Self = NonCopyableType;
    NonCopyableType(std::int64_t value) : value_(value) {}

    NonCopyableType()            = default;
    NonCopyableType(const Self&) = delete;
    NonCopyableType(Self&&)      = default;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = default;

    std::string Str() const { return std::to_string(value_); }

    std::int64_t value_{0};
};

using NonCopyableTypePtr = std::shared_ptr<NonCopyableType>;

inline void ConfigDataParser(NonCopyableType& data, simdjson::ondemand::value& val) {
    std::int64_t res{};
    const auto   error = val.get(res);
    EXPECT_FALSE(error);
    data = NonCopyableType(res);
}

inline void ConfigDataParser(NonCopyableTypePtr& data, simdjson::ondemand::value& val) {
    data = std::make_shared<NonCopyableType>();
    ConfigDataParser(*data, val);
}

}  // namespace inner

TEST_F(ConfigTest, NonCopyableTypeTest) {
    const std::string custom_type1_config_name = "noncopyable_type";
    std::int64_t      value1                   = 11;
    const std::string custom_type2_config_name = "noncopyable_type_ptr";
    std::int64_t      value2                   = 12;

    auto config_file = MakeConfigFile(fmt::format(
        R"({{
            "{}": {},
            "{}": {}
        }})",
        custom_type1_config_name,
        value1,
        custom_type2_config_name,
        value2));

    const std::int64_t defualt_int_value = 100;
    EXPECT_EQ(
        config_file.Get<inner::NonCopyableType>("Do not care about name", inner::NonCopyableType(defualt_int_value))
            ->value_,
        defualt_int_value);
    EXPECT_EQ(
        config_file
            .Get<inner::NonCopyableTypePtr>(
                "Do not care about name",
                std::make_shared<inner::NonCopyableType>(defualt_int_value))
            ->get()
            ->value_,
        defualt_int_value);

    EXPECT_EQ(config_file.Get<inner::NonCopyableType>(custom_type1_config_name)->value_, value1);
    EXPECT_EQ(config_file.Get<inner::NonCopyableTypePtr>(custom_type2_config_name)->get()->value_, value2);
}

TEST_F(ConfigTest, ConfigDataGetStringRep) {
    // Just to make sure it compiles in the various cases.
    // We do not care about the result. Non-empty should be enough.
    EXPECT_FALSE(impl::ConfigDataGetStringRep(std::string()).empty());
    EXPECT_FALSE(impl::ConfigDataGetStringRep(std::vector<int>()).empty());
    EXPECT_FALSE(impl::ConfigDataGetStringRep(std::map<int, int>()).empty());

    struct SomeRandomClass {};
    EXPECT_FALSE(impl::ConfigDataGetStringRep(SomeRandomClass()).empty());
}

}  // namespace cris::core
