#include "cris/core/msg_recorder/record_file.h"
#include "cris/core/msg_recorder/recorder.h"
#include "cris/core/msg_recorder/recorder_config.h"
#include "cris/core/utils/time.h"

#include <gmock/gmock.h>

#include <gtest/gtest.h>

#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace testing;

namespace cris::core {

class RecordFileTestFixture : public Test {
   public:
    RecordFileTestFixture()
        : sandbox_dir_{fs::temp_directory_path() / (std::string{"CRIS.record_rolling_test."} + std::to_string(getpid()))}
        , record_dir_{sandbox_dir_ / "msg-type/subid"} {}

    ~RecordFileTestFixture() override { fs::remove_all(sandbox_dir_); }

    const fs::path& GetRecordDir() const { return record_dir_; }

   protected:
    const fs::path sandbox_dir_;
    const fs::path record_dir_;
};

TEST_F(RecordFileTestFixture, Symlink_OK) {
    const std::string filename = "20230323T010000";
    const std::string linkname = "link-to-subid";
    const fs::path    filepath = GetRecordDir() / filename;
    RecordFile        record_file{filepath.native(), linkname};

    EXPECT_TRUE(fs::is_directory(filepath));
    EXPECT_TRUE(record_file.IsOpen());

    const fs::path linkpath = sandbox_dir_ / linkname;
    EXPECT_TRUE(fs::is_symlink(linkpath));
    EXPECT_EQ(fs::read_symlink(linkpath), "msg-type/subid")
        << ": Symlink " << linkpath << " points to an unexpected path.";
}

TEST_F(RecordFileTestFixture, CloseDB_RemoveEmptyDB) {
    const std::string filename    = "20230323T010000";
    const std::string linkname    = "link-to-subid";
    const fs::path    filepath    = GetRecordDir() / filename;
    auto              record_file = std::make_unique<RecordFile>(filepath.native(), linkname);

    EXPECT_TRUE(fs::is_directory(filepath));
    EXPECT_TRUE(record_file->IsOpen());

    record_file.reset();
    EXPECT_FALSE(fs::exists(filepath));
    EXPECT_TRUE(fs::exists(filepath.parent_path()));
}

TEST_F(RecordFileTestFixture, CloseDB_KeepNonEmptyDB) {
    const std::string filename    = "20230323T010000";
    const std::string linkname    = "link-to-subid";
    const fs::path    filepath    = GetRecordDir() / filename;
    auto              record_file = std::make_unique<RecordFile>(filepath.native(), linkname);

    EXPECT_TRUE(fs::is_directory(filepath));
    EXPECT_TRUE(record_file->IsOpen());

    record_file->Write("abc");

    record_file.reset();
    EXPECT_TRUE(fs::exists(filepath));
}

class MockRollingHelper : public RollingHelper {
   public:
    MOCK_METHOD(bool, NeedToRoll, (const Metadata& metadata), (const, override));
    MOCK_METHOD(void, Update, (const Metadata& metadata), (override));
    MOCK_METHOD(void, Reset, (), (override));
    MOCK_METHOD(std::string, MakeNewRecordDirName, (), (const, override));
};

TEST_F(RecordFileTestFixture, NoNeedToRoll_OK) {
    const std::string filename = "20230323T010000";
    const std::string linkname = "link-to-subid";
    const fs::path    filepath = GetRecordDir() / filename;

    auto  mock_rolling_helper_ptr = std::make_unique<MockRollingHelper>();
    auto& mock_rolling_helper     = *mock_rolling_helper_ptr;

    EXPECT_CALL(mock_rolling_helper, NeedToRoll(_)).Times(1).WillOnce(Return(false));
    EXPECT_CALL(mock_rolling_helper, Update(_)).Times(1);
    EXPECT_CALL(mock_rolling_helper, Reset()).Times(0);
    EXPECT_CALL(mock_rolling_helper, MakeNewRecordDirName()).Times(0);

    RecordFile record_file{filepath.native(), linkname, std::move(mock_rolling_helper_ptr)};
    record_file.Write("abc");
    EXPECT_EQ(filepath, record_file.GetFilePath());
}

TEST_F(RecordFileTestFixture, Roll_OK) {
    const std::string filename      = "20230323T010000";
    const std::string linkname      = "link-to-subid";
    const fs::path    filepath      = GetRecordDir() / filename;
    const fs::path    next_filepath = GetRecordDir() / "20230324T010000";

    auto  mock_rolling_helper_ptr = std::make_unique<MockRollingHelper>();
    auto& mock_rolling_helper     = *mock_rolling_helper_ptr;

    EXPECT_CALL(mock_rolling_helper, NeedToRoll(_)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(mock_rolling_helper, Update(_)).Times(1);
    EXPECT_CALL(mock_rolling_helper, Reset()).Times(1);
    EXPECT_CALL(mock_rolling_helper, MakeNewRecordDirName()).Times(1).WillOnce(Return(next_filepath.native()));

    constexpr std::string_view value{"abc"};
    RecordFile                 record_file{filepath.native(), linkname, std::move(mock_rolling_helper_ptr)};
    const auto                 key = RecordFileKey::Make();
    record_file.Write(key, std::string{value});

    // Old DB is empty, expected to be removed
    EXPECT_FALSE(fs::exists(filepath));
    EXPECT_TRUE(fs::is_directory(next_filepath));

    EXPECT_EQ(next_filepath, record_file.GetFilePath());
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
