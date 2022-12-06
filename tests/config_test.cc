#include "cris/core/config/config.h"

#include "cris/core/config/recorder_config.h"

#include <gtest/gtest.h>

#include <fmt/core.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace cris::core {

namespace fs = std::filesystem;

class ConfigTest : public testing::Test {
   public:
    ConfigTest()
        : testing::Test()
        , test_config_path_(fs::temp_directory_path() / fmt::format("config_test.pid.{}.json", getpid())) {}

    ~ConfigTest() { fs::remove(test_config_path_); }

    ConfigFile MakeConfigFile(std::string content) const {
        std::ofstream config_file(test_config_path_);
        config_file << std::move(content);
        config_file.flush();
        return ConfigFile(test_config_path_);
    }

   private:
    fs::path test_config_path_;
};

TEST_F(ConfigTest, Configs) {
    const std::string int_key      = "int_config";
    int               int_val      = 123;
    const std::string large_fp_key = "large_double_config";
    double            large_fp_val = 1e300;
    const std::string hp_fp_key    = "high_precision_double_config";
    double            hp_fp_val    = 1 + 1e-15;
    const std::string bool_key     = "bool_config";
    bool              bool_val     = true;

    auto config_file = MakeConfigFile(fmt::format(
        R"({{
            "{}": {},
            "{}": {},
            "{}": {},
            "{}": {}
        }})",
        int_key,
        int_val,
        large_fp_key,
        large_fp_val,
        hp_fp_key,
        hp_fp_val,
        bool_key,
        bool_val));

    // There is a value in the config file, the default value will be ignored.
    EXPECT_EQ(config_file.Get<int>(int_key, /* default_value = */ 0)->GetValue(), int_val);

    EXPECT_DOUBLE_EQ(config_file.Get<double>(large_fp_key)->GetValue(), large_fp_val);
    EXPECT_DOUBLE_EQ(config_file.Get<double>(hp_fp_key)->GetValue(), hp_fp_val);
    EXPECT_EQ(config_file.Get<bool>(bool_key)->GetValue(), bool_val);

    // No value in the config file, so the value passed to Register will be picked up.
    {
        const std::string missing_key  = "A key that is missing in the config";
        int               default_val1 = 100;
        EXPECT_EQ(config_file.Get<int>(missing_key, default_val1)->GetValue(), default_val1);
        int default_val2 = 200;
        EXPECT_EQ(config_file.Get<int>(missing_key, default_val2)->GetValue(), default_val2);
    }

    EXPECT_DEATH(config_file.Get<double>(int_key), "Type mismatched.");
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
            ->GetValue()
            .value_,
        defualt_int_value);
    EXPECT_EQ(
        config_file
            .Get<inner::NonCopyableTypePtr>(
                "Do not care about name",
                std::make_shared<inner::NonCopyableType>(defualt_int_value))
            ->GetValue()
            ->value_,
        defualt_int_value);

    EXPECT_EQ(config_file.Get<inner::NonCopyableType>(custom_type1_config_name)->GetValue().value_, value1);
    EXPECT_EQ(config_file.Get<inner::NonCopyableTypePtr>(custom_type2_config_name)->GetValue()->value_, value2);
}

TEST_F(ConfigTest, ConfigDataGetStringRep) {
    // Just to make sure it compiles in various cases.
    // We do not care about the result. Non-empty should be enough.
    EXPECT_FALSE(impl::ConfigDataGetStringRep(std::string()).empty());
    EXPECT_FALSE(impl::ConfigDataGetStringRep(std::vector<int>()).empty());
    EXPECT_FALSE(impl::ConfigDataGetStringRep(std::map<int, int>()).empty());

    struct SomeRandomClass {};
    EXPECT_FALSE(impl::ConfigDataGetStringRep(SomeRandomClass()).empty());
}

TEST_F(ConfigTest, WrongFilePath) {
    ConfigFile empty_config("");
    ConfigFile invalid_file_config("nonexistent.json");

    int default_val = 100;
    EXPECT_EQ(empty_config.Get<int>("key", default_val)->GetValue(), default_val);
    EXPECT_EQ(invalid_file_config.Get<int>("key", default_val)->GetValue(), default_val);
}

class RecordConfigTest : public testing::Test {
   public:
    RecordConfigTest()
        : testing::Test()
        , test_config_path_(fs::temp_directory_path() / fmt::format("record_config_test.pid.{}.json", getpid())) {}

    ~RecordConfigTest() { fs::remove(test_config_path_); }

    ConfigFile MakeRecordConfigFile(std::string content) const {
        std::ofstream config_file(test_config_path_);
        config_file << std::move(content);
        config_file.flush();
        return ConfigFile(test_config_path_);
    }

    fs::path GetRecordConfigPath() const { return test_config_path_; };

   private:
    fs::path test_config_path_;
};

TEST_F(RecordConfigTest, RecorderConfigTestBasic) {
    auto recorder_config_file = MakeRecordConfigFile(fmt::format(
        R"({{
            "recorder": {{
                "snapshot_intervals_sec" : [
                    {{
                        "interval_name": "SECONDLY",
                        "interval_sec": 5
                    }},
                    {{
                        "interval_name": "HOURLY",
                        "interval_sec": 1
                    }}
                ],
                "record_dir": "record_test"
            }}
        }})"));

    RecorderConfig recorder_config = recorder_config_file.Get<RecorderConfig>("recorder")->GetValue();

    EXPECT_EQ(recorder_config.snapshot_intervals_.size(), 2);
    EXPECT_EQ(recorder_config.snapshot_intervals_.front().name_, "SECONDLY");
    EXPECT_EQ(recorder_config.snapshot_intervals_.front().interval_sec_, std::chrono::seconds(5));
    EXPECT_EQ(recorder_config.snapshot_intervals_.back().name_, "HOURLY");
    EXPECT_EQ(recorder_config.snapshot_intervals_.back().interval_sec_, std::chrono::seconds(1));
    EXPECT_EQ(recorder_config.record_dir_, "record_test");
}

TEST_F(RecordConfigTest, RecorderConfigTestInvalid) {
    auto config_improper_structure = MakeRecordConfigFile(fmt::format(
        R"({{
            "recorder": {{
                "snapshot_intervals_sec" : [
                    {{
                        "interval_name": "SECONDLY",
                    }},
                    {{
                        "interval_name": "HOURLY",
                        "interval_sec": 1
                    }}
                ],
                "record_dir": "record_test"
            }}
        }})"));

    // simdjson::error_code::TAPE_ERROR
    EXPECT_DEATH(
        config_improper_structure.Get<RecorderConfig>("recorder")->GetValue(),
        "The JSON document has an improper structure");

    auto config_missing_type = MakeRecordConfigFile(fmt::format(
        R"({{
            "recorder": {{
                "snapshot_intervals_sec" : [
                    {{
                        "interval_name": "SECONDLY",
                        "interval_sec": 5
                    }}
                ],
                "record_dir": 
            }}
        }})"));

    // simdjson::error_code::INCORRECT_TYPE
    EXPECT_DEATH(
        config_missing_type.Get<RecorderConfig>("recorder")->GetValue(),
        "\"record_dir\" is required. The JSON element does not have the requested type");

    auto config_missing_field = MakeRecordConfigFile(fmt::format(
        R"({{
        "recorder": {{
        }}
    }})"));

    // simdjson::error_code::NO_SUCH_FIELD
    EXPECT_DEATH(
        config_missing_field.Get<RecorderConfig>("recorder")->GetValue(),
        "The JSON field referenced does not exist in this object");
}

TEST_F(RecordConfigTest, RecorderConfigTestEmpty) {
    auto config_file_empty = MakeRecordConfigFile(fmt::format(
        R"({{
        }})"));

    // use default value instead
    RecorderConfig recorder_config = config_file_empty.Get<RecorderConfig>("recorder")->GetValue();
    EXPECT_EQ(recorder_config.snapshot_intervals_.size(), 0);
    EXPECT_EQ(recorder_config.record_dir_, "");
}

}  // namespace cris::core
