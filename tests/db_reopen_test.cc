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

class RecorderPauseTest : public testing::Test {
   public:
    RecorderPauseTest() : testing::Test() { std::filesystem::create_directories(test_temp_dir_); }

    ~RecorderPauseTest() { std::filesystem::remove_all(test_temp_dir_); }

    std::filesystem::path GetRecordFilePath() { return record_file_path; }

    static constexpr int kMessageNum              = 10;
    static constexpr int kMessageNumBetweenReopen = 2;

   private:
    const std::string     filename = std::string("test_filename");
    std::filesystem::path test_temp_dir_{
        std::filesystem::temp_directory_path() / (std::string("CRPauseTestTmpDir.") + std::to_string(getpid()))};
    const std::filesystem::path record_file_path = test_temp_dir_ / filename;
};

TEST_F(RecorderPauseTest, RecorderPauseTest) {
    std::filesystem::create_directories(GetRecordFilePath());
    auto record_file = std::make_unique<RecordFile>(GetRecordFilePath());

    // repeatedly open and close the DB
    for (int i = 0; i < kMessageNum; ++i) {
        if (i % kMessageNumBetweenReopen == 0) {
            record_file->CloseDB();
            record_file->OpenDB();
        }
        record_file->Write(std::to_string(i));
    }

    int expected_value = 0;
    for (auto itr = record_file->Iterate(); itr.Valid(); itr.Next(), ++expected_value) {
        int current_value = std::stoi(itr.Get().second);
        EXPECT_EQ(current_value, expected_value);
    }
}

}  // namespace cris::core
