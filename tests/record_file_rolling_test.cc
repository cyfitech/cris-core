#include "cris/core/msg_recorder/record_file.h"
#include "cris/core/msg_recorder/recorder.h"
#include "cris/core/msg_recorder/recorder_config.h"
#include "cris/core/utils/time.h"

#include <gmock/gmock.h>

#include <gtest/gtest.h>

#include <unistd.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace fs = std::filesystem;
using namespace testing;

namespace cris::core {

class RecordFileTestFixture : public Test {
   public:
    RecordFileTestFixture()
        : sandbox_dir_{fs::temp_directory_path() / (std::string{"CRIS.record_rolling_test."} + std::to_string(getpid()))}
        , record_dir_{sandbox_dir_ / kHost / "msg-type/subid"} {}

    ~RecordFileTestFixture() override { fs::remove_all(sandbox_dir_); }

    const fs::path& GetRecordDir() const { return record_dir_; }

   protected:
    static constexpr std::string_view kHost = "host";

    const fs::path sandbox_dir_;
    const fs::path record_dir_;
};

TEST_F(RecordFileTestFixture, OpenNewDB_OK) {
    const std::string filename    = "20230323T010000";
    const std::string linkname    = "link-to-subid";
    const fs::path    filepath    = GetRecordDir() / filename;
    auto              record_file = std::make_unique<RecordFile>(filepath.native(), linkname);

    ASSERT_TRUE(record_file);
    EXPECT_TRUE(record_file->IsOpen());
    EXPECT_TRUE(record_file->Empty());

    EXPECT_FALSE(fs::exists(filepath));
    const std::string expected_path = RecordFile::UnfinishedPath(filepath.native());
    EXPECT_EQ(expected_path, record_file->GetFilePath());
    EXPECT_TRUE(fs::is_directory(expected_path));

    const fs::path linkpath = sandbox_dir_ / kHost / linkname;
    EXPECT_TRUE(fs::is_symlink(linkpath));
    const auto link_target = fs::read_symlink(linkpath);
    EXPECT_EQ(link_target, "msg-type/subid") << ": link " << linkpath << " points to " << link_target;

    record_file->Write("abc");

    record_file.reset();
    EXPECT_FALSE(fs::exists(expected_path));
    EXPECT_TRUE(fs::is_directory(filepath)) << ": Path " << filepath << " is not a directory.";
    EXPECT_TRUE(fs::is_symlink(linkpath)) << ": Path " << linkpath << " is not a symlink.";
}

TEST_F(RecordFileTestFixture, OpenExistedDB_OK) {
    const std::string filename = "20230323T010000";
    const std::string linkname = "link-to-subid";
    const fs::path    filepath = GetRecordDir() / filename;

    {
        auto record_file = std::make_unique<RecordFile>(filepath.native(), linkname);

        ASSERT_TRUE(record_file);
        EXPECT_TRUE(record_file->IsOpen());
        EXPECT_TRUE(record_file->Empty());

        EXPECT_FALSE(fs::exists(filepath));
        const std::string expected_path = RecordFile::UnfinishedPath(filepath.native());
        EXPECT_EQ(expected_path, record_file->GetFilePath());
        EXPECT_TRUE(fs::is_directory(expected_path));

        const fs::path linkpath = sandbox_dir_ / kHost / linkname;
        EXPECT_TRUE(fs::is_symlink(linkpath));
        const auto link_target = fs::read_symlink(linkpath);
        EXPECT_EQ(link_target, "msg-type/subid") << ": link " << linkpath << " points to " << link_target;

        record_file->Write("abc");

        record_file.reset();
        EXPECT_FALSE(fs::exists(expected_path));
        EXPECT_TRUE(fs::is_directory(filepath)) << ": Path " << filepath << " is not a directory.";
        EXPECT_TRUE(fs::is_symlink(linkpath)) << ": Path " << linkpath << " is not a symlink.";
    }

    // Open existing DB
    {
        auto record_file = std::make_unique<RecordFile>(filepath.native(), linkname);

        ASSERT_TRUE(record_file);
        EXPECT_TRUE(record_file->IsOpen());
        EXPECT_FALSE(record_file->Empty());

        EXPECT_FALSE(fs::exists(filepath));
        const std::string expected_path = RecordFile::UnfinishedPath(filepath.native());
        EXPECT_EQ(expected_path, record_file->GetFilePath());
        EXPECT_TRUE(fs::is_directory(expected_path));

        const fs::path linkpath = sandbox_dir_ / kHost / linkname;
        EXPECT_TRUE(fs::is_symlink(linkpath));
        const auto link_target = fs::read_symlink(linkpath);
        EXPECT_EQ(link_target, "msg-type/subid") << ": link " << linkpath << " points to " << link_target;

        record_file.reset();
        EXPECT_FALSE(fs::exists(expected_path));
        EXPECT_TRUE(fs::is_directory(filepath)) << ": Path " << filepath << " is not a directory.";
        EXPECT_TRUE(fs::is_symlink(linkpath)) << ": Path " << linkpath << " is not a symlink.";
    }
}

TEST_F(RecordFileTestFixture, CloseDB_RemoveEmptyDB) {
    const std::string filename    = "20230323T010000";
    const std::string linkname    = "link-to-subid";
    const fs::path    filepath    = GetRecordDir() / filename;
    auto              record_file = std::make_unique<RecordFile>(filepath.native(), linkname);

    ASSERT_TRUE(record_file);
    EXPECT_TRUE(record_file->IsOpen());
    EXPECT_TRUE(record_file->Empty());

    EXPECT_FALSE(fs::exists(filepath));
    EXPECT_TRUE(fs::is_directory(RecordFile::UnfinishedPath(filepath.native())));

    record_file.reset();
    EXPECT_FALSE(fs::exists(filepath));
    EXPECT_FALSE(fs::exists(RecordFile::UnfinishedPath(filepath.native())));
    EXPECT_TRUE(fs::exists(filepath.parent_path()));
}

TEST_F(RecordFileTestFixture, CloseDB_KeepNonEmptyDB) {
    const std::string filename    = "20230323T010000";
    const std::string linkname    = "link-to-subid";
    const fs::path    filepath    = GetRecordDir() / filename;
    auto              record_file = std::make_unique<RecordFile>(filepath.native(), linkname);

    ASSERT_TRUE(record_file);
    EXPECT_TRUE(record_file->IsOpen());
    EXPECT_TRUE(record_file->Empty());

    EXPECT_FALSE(fs::exists(filepath));
    EXPECT_TRUE(fs::is_directory(RecordFile::UnfinishedPath(filepath.native())));

    record_file->Write("abc");
    EXPECT_FALSE(record_file->Empty());

    record_file.reset();
    EXPECT_TRUE(fs::is_directory(filepath));
    EXPECT_FALSE(fs::exists(RecordFile::UnfinishedPath(filepath.native())));
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

    auto record_file = std::make_unique<RecordFile>(filepath.native(), linkname, std::move(mock_rolling_helper_ptr));
    ASSERT_TRUE(record_file);
    EXPECT_TRUE(record_file->IsOpen());

    record_file->Write("abc");
    EXPECT_EQ(RecordFile::UnfinishedPath(filepath.native()), record_file->GetFilePath());
    EXPECT_FALSE(fs::exists(filepath));
    EXPECT_TRUE(fs::is_directory(RecordFile::UnfinishedPath(filepath.native())));

    record_file.reset();
    EXPECT_TRUE(fs::is_directory(filepath));
    EXPECT_FALSE(fs::exists(RecordFile::UnfinishedPath(filepath.native())));
}

TEST_F(RecordFileTestFixture, Roll_OK) {
    const std::string filename      = "20230323T010000";
    const std::string linkname      = "link-to-subid";
    const fs::path    linkpath      = sandbox_dir_ / kHost / linkname;
    const fs::path    filepath      = GetRecordDir() / filename;
    const fs::path    next_filepath = GetRecordDir() / "20230324T010000";

    const auto                 key1 = RecordFileKey::Make();
    constexpr std::string_view value1{"abc"};
    const auto                 key2 = RecordFileKey::Make();
    constexpr std::string_view value2{"def"};

    auto  mock_rolling_helper_ptr = std::make_unique<MockRollingHelper>();
    auto& mock_rolling_helper     = *mock_rolling_helper_ptr;

    EXPECT_CALL(mock_rolling_helper, NeedToRoll(_)).Times(2).WillOnce(Return(false)).WillOnce(Return(true));
    EXPECT_CALL(mock_rolling_helper, Update(_)).Times(2);
    EXPECT_CALL(mock_rolling_helper, Reset()).Times(1);
    EXPECT_CALL(mock_rolling_helper, MakeNewRecordDirName()).Times(1).WillOnce(Return(next_filepath.native()));

    {
        auto record_file =
            std::make_unique<RecordFile>(filepath.native(), linkname, std::move(mock_rolling_helper_ptr));
        ASSERT_TRUE(record_file);
        EXPECT_TRUE(record_file->IsOpen());
        EXPECT_TRUE(record_file->Empty());

        EXPECT_FALSE(fs::is_directory(filepath));
        EXPECT_TRUE(fs::is_directory(RecordFile::UnfinishedPath(filepath.native())));

        EXPECT_TRUE(fs::is_symlink(linkpath));
        const auto link_target = fs::read_symlink(linkpath);
        EXPECT_EQ(link_target, "msg-type/subid") << ": link " << linkpath << " points to " << link_target;
        record_file->Write(key1, std::string{value1});

        record_file->Write(key2, std::string{value2});

        EXPECT_TRUE(fs::is_directory(filepath));
        EXPECT_FALSE(fs::exists(RecordFile::UnfinishedPath(filepath.native())));

        EXPECT_FALSE(fs::exists(next_filepath));
        const fs::path actual_next_filepath{RecordFile::UnfinishedPath(next_filepath.native())};
        EXPECT_TRUE(fs::is_directory(actual_next_filepath));
        EXPECT_EQ(actual_next_filepath.native(), record_file->GetFilePath());
        EXPECT_TRUE(record_file->IsOpen());
        EXPECT_FALSE(record_file->Empty());

        {
            auto db_iter = record_file->Iterate();
            EXPECT_TRUE(db_iter.Valid());

            const auto& [key_read, value_read] = db_iter.Get();
            EXPECT_EQ(RecordFileKey::compare(key_read, key2), 0)
                << "Expect key: " << key2.ToBytes() << ", but actual: " << key_read.ToBytes();
            EXPECT_EQ(value2, value_read) << "Expect value: " << value2 << ", but actual: " << value_read;

            db_iter.Next();
            EXPECT_FALSE(db_iter.Valid());
        }

        record_file.reset();
        EXPECT_TRUE(fs::exists(filepath));
        EXPECT_TRUE(fs::exists(next_filepath));
    }

    // Open the first DB
    {
        auto record_file =
            std::make_unique<RecordFile>(filepath.native(), linkname, std::move(mock_rolling_helper_ptr));
        ASSERT_TRUE(record_file);
        EXPECT_TRUE(record_file->IsOpen());
        EXPECT_FALSE(record_file->Empty());

        auto db_iter = record_file->Iterate();
        EXPECT_TRUE(db_iter.Valid());

        const auto& [key_read, value_read] = db_iter.Get();
        EXPECT_EQ(RecordFileKey::compare(key_read, key1), 0)
            << "Expect key: " << key1.ToBytes() << ", but actual: " << key_read.ToBytes();
        EXPECT_EQ(value1, value_read) << "Expect value: " << value1 << ", but actual: " << value_read;

        db_iter.Next();
        EXPECT_FALSE(db_iter.Valid());
    }
}

TEST_F(RecordFileTestFixture, Roll_OK_RemoveEmptyDB) {
    const std::string filename      = "20230323T010000";
    const std::string linkname      = "link-to-subid";
    const fs::path    linkpath      = sandbox_dir_ / kHost / linkname;
    const fs::path    filepath      = GetRecordDir() / filename;
    const fs::path    next_filepath = GetRecordDir() / "20230324T010000";

    auto  mock_rolling_helper_ptr = std::make_unique<MockRollingHelper>();
    auto& mock_rolling_helper     = *mock_rolling_helper_ptr;

    EXPECT_CALL(mock_rolling_helper, NeedToRoll(_)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(mock_rolling_helper, Update(_)).Times(1);
    EXPECT_CALL(mock_rolling_helper, Reset()).Times(1);
    EXPECT_CALL(mock_rolling_helper, MakeNewRecordDirName()).Times(1).WillOnce(Return(next_filepath.native()));

    auto record_file = std::make_unique<RecordFile>(filepath.native(), linkname, std::move(mock_rolling_helper_ptr));
    ASSERT_TRUE(record_file);
    EXPECT_TRUE(record_file->IsOpen());

    EXPECT_TRUE(fs::is_symlink(linkpath));
    const auto link_target = fs::read_symlink(linkpath);
    EXPECT_EQ(link_target, "msg-type/subid") << ": link " << linkpath << " points to " << link_target;

    const auto                 key = RecordFileKey::Make();
    constexpr std::string_view value{"abc"};
    record_file->Write(key, std::string{value});

    // Old DB is empty, expected to be removed
    EXPECT_FALSE(fs::exists(filepath));
    EXPECT_FALSE(fs::exists(RecordFile::UnfinishedPath(filepath.native())));

    EXPECT_FALSE(fs::exists(next_filepath));
    const fs::path actual_next_filepath{RecordFile::UnfinishedPath(next_filepath.native())};
    EXPECT_TRUE(fs::is_directory(actual_next_filepath));
    EXPECT_EQ(actual_next_filepath.native(), record_file->GetFilePath());
    EXPECT_TRUE(record_file->IsOpen());
    EXPECT_FALSE(record_file->Empty());

    {
        auto db_iter = record_file->Iterate();
        EXPECT_TRUE(db_iter.Valid());

        const auto& [key_read, value_read] = db_iter.Get();
        EXPECT_EQ(RecordFileKey::compare(key_read, key), 0)
            << "Expect key: " << key.ToBytes() << ", but actual: " << key_read.ToBytes();
        EXPECT_EQ(value, value_read);

        db_iter.Next();
        EXPECT_FALSE(db_iter.Valid());
    }

    record_file.reset();
    EXPECT_TRUE(fs::is_directory(next_filepath));
}

}  // namespace cris::core
