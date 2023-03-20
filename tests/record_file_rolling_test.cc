#include "cris/core/msg_recorder/record_file.h"
#include "cris/core/msg_recorder/recorder.h"
#include "cris/core/msg_recorder/recorder_config.h"
#include "cris/core/utils/time.h"

#include <gmock/gmock.h>

#include <gtest/gtest.h>

#include <fmt/core.h>

#include <vector>

namespace fs = std::filesystem;
using namespace testing;

namespace cris::core {

namespace {
constexpr std::string_view kHostname = "test-host";
constexpr std::string_view kLocation = "test-location";
}  // namespace

class RecordFileTestFixture : public ::testing::Test {
   public:
    RecordFileTestFixture()
        : temp_test_dir_{fs::temp_directory_path() / fmt::format("CRIS.record_file_rolling_test.{}", getpid())}
        , daily_rolling_dir_path_generator_{[this] {
            return GetTopDir() / GetRecordSubDirName(RecorderConfig::Rolling::kDay) / kHostname / kLocation;
        }} {}

    ~RecordFileTestFixture() override { fs::remove_all(GetTopDir()); }

    const fs::path& GetTopDir() const { return temp_test_dir_; }

   protected:
    const fs::path                              temp_test_dir_;
    const RollingHelper::RecordDirPathGenerator daily_rolling_dir_path_generator_;
};

TEST_F(RecordFileTestFixture, Symlink_OK) {
    const std::string filename = "dummy";
    const std::string linkname = "link-to-dummy";
    const fs::path    filepath = daily_rolling_dir_path_generator_() / filename;
    RecordFile        record_file{filepath.native(), linkname};

    EXPECT_TRUE(fs::is_directory(filepath));
    EXPECT_TRUE(record_file.IsOpen());

    const fs::path linkpath = daily_rolling_dir_path_generator_() / linkname;
    EXPECT_TRUE(fs::is_symlink(linkpath));
}

class MockRollingHelper : public RollingHelper {
   public:
    explicit MockRollingHelper(const RecordDirPathGenerator* const dir_path_maker) : RollingHelper{dir_path_maker} {}

    MOCK_METHOD(bool, NeedToRoll, (const Metadata& metadata), (const, override));
    MOCK_METHOD(void, Update, (const Metadata& metadata), (override));
    MOCK_METHOD(void, Reset, (), (override));
    MOCK_METHOD(std::filesystem::path, GenerateFullRecordDirPath, (), (const));
};

TEST_F(RecordFileTestFixture, NoNeedToRoll_OK) {
    const std::string filename = "dummy";
    const std::string linkname = "link-to-dummy";
    const fs::path    filepath = daily_rolling_dir_path_generator_() / filename;

    auto  mock_rolling_helper_ptr = std::make_unique<MockRollingHelper>(&daily_rolling_dir_path_generator_);
    auto& mock_rolling_helper     = *mock_rolling_helper_ptr;

    EXPECT_CALL(mock_rolling_helper, NeedToRoll(_)).Times(1).WillOnce(Return(false));
    EXPECT_CALL(mock_rolling_helper, Update(_)).Times(1);
    EXPECT_CALL(mock_rolling_helper, Reset()).Times(0);
    EXPECT_CALL(mock_rolling_helper, GenerateFullRecordDirPath()).Times(0);

    RecordFile record_file{filepath.native(), linkname, std::move(mock_rolling_helper_ptr)};
    record_file.Write("abc");
    EXPECT_EQ(filepath.native(), record_file.GetFilePath());
}

TEST_F(RecordFileTestFixture, Roll_OK) {
    const std::string filename     = "dummy";
    const std::string linkname     = "link-to-dummy";
    const fs::path    filepath     = daily_rolling_dir_path_generator_() / filename;
    const fs::path    next_dirpath = GetTopDir() / "rolling-next";

    auto  mock_rolling_helper_ptr = std::make_unique<MockRollingHelper>(&daily_rolling_dir_path_generator_);
    auto& mock_rolling_helper     = *mock_rolling_helper_ptr;

    EXPECT_CALL(mock_rolling_helper, NeedToRoll(_)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(mock_rolling_helper, Update(_)).Times(1);
    EXPECT_CALL(mock_rolling_helper, Reset()).Times(1);
    EXPECT_CALL(mock_rolling_helper, GenerateFullRecordDirPath()).Times(1).WillOnce(Return(next_dirpath));

    constexpr std::string_view value{"abc"};
    RecordFile                 record_file{filepath.native(), linkname, std::move(mock_rolling_helper_ptr)};
    const auto                 key = RecordFileKey::Make();
    record_file.Write(key, std::string{value});

    const fs::path latest_record_path = next_dirpath / filename;
    EXPECT_TRUE(fs::is_directory(latest_record_path));
    EXPECT_TRUE(fs::is_symlink(next_dirpath / linkname));

    const std::string expected_filepath = fs::path{next_dirpath / filename}.native();
    EXPECT_EQ(expected_filepath, record_file.GetFilePath());
    EXPECT_TRUE(record_file.IsOpen());
    EXPECT_FALSE(record_file.Empty());

    auto db_iter = record_file.Iterate();
    EXPECT_TRUE(db_iter.Valid());

    const auto& [key_read, value_read] = db_iter.Get();
    EXPECT_EQ(RecordFileKey::compare(key_read, key), 0)
        << "Expect key: " << key.ToBytes() << ", but actual: " << key_read.ToBytes();
    EXPECT_EQ(value, value_read);
}

}  // namespace cris::core
