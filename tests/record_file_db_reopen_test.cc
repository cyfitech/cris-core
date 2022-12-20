#include "cris/core/msg_recorder/record_file.h"

#include "gtest/gtest.h"

#include <unistd.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace cris::core {

class RecordFileDbReopenTest : public testing::Test {
   public:
    RecordFileDbReopenTest() { std::filesystem::create_directories(test_temp_dir_); }

    ~RecordFileDbReopenTest() override { std::filesystem::remove_all(test_temp_dir_); }

    std::filesystem::path GetRecordFilePath() const { return record_file_path; }

    static constexpr std::size_t kMessageNum              = 10;
    static constexpr std::size_t kMessageNumBetweenReopen = 2;

   private:
    const std::string     filename = std::string("test_filename");
    std::filesystem::path test_temp_dir_{
        std::filesystem::temp_directory_path() / (std::string("CRPauseTestTmpDir.") + std::to_string(getpid()))};
    const std::filesystem::path record_file_path = test_temp_dir_ / filename;
};

TEST_F(RecordFileDbReopenTest, RecordFileDbReopenTest) {
    std::filesystem::create_directories(GetRecordFilePath());
    auto record_file = std::make_unique<RecordFile>(GetRecordFilePath());

    // repeatedly open and close the DB
    for (std::size_t i = 0; i < kMessageNum; ++i) {
        if (i % kMessageNumBetweenReopen == 0) {
            record_file->CloseDB();
            record_file->OpenDB();
        }
        record_file->Write(std::to_string(i));
    }

    std::size_t expected_value = 0;
    for (auto itr = record_file->Iterate(); itr.Valid(); itr.Next(), ++expected_value) {
        std::size_t current_value = static_cast<std::size_t>(std::stoi(itr.Get().second));
        EXPECT_EQ(current_value, expected_value);
    }
}

}  // namespace cris::core
