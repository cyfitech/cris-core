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

    static constexpr std::size_t kMessageNum = 10;
    static constexpr std::size_t kManipulatePatternNum = 2;

   private:
    const std::string exchange_symbol_str = std::string("TEST_EXCHANGE") + "__" + std::string("SYMBOL");
    const std::string filename            = exchange_symbol_str + "_id_0.ldb.d";
    std::filesystem::path test_temp_dir_{
        std::filesystem::temp_directory_path() / (std::string("CRPauseTestTmpDir.") + std::to_string(getpid()))};
    const std::filesystem::path record_file_path = test_temp_dir_ / filename;
};

TEST_F(RecorderPauseTest, RecorderPauseTest) {
    std::filesystem::create_directories(GetRecordFilePath());
    auto record_file = std::make_unique<RecordFile>(GetRecordFilePath());

    // repeatedly open and close the DB
    for (std::size_t i = 0; i < kMessageNum; ++i) {
        if (i % kManipulatePatternNum == 0) {
            record_file->CloseDB();
            record_file->OpenDB();
        }
        record_file->Write(std::to_string(i));
    }

    int previous_int  = -1;
    std::size_t int_msg_count = 0;

    for (auto itr = record_file->Iterate(); itr.Valid(); itr.Next(), ++int_msg_count) {
        int current_value = std::stoi(itr.Get().second);

        // first element expect to be zero
        if (int_msg_count == 0) {
            EXPECT_EQ(current_value, 0);
        }

        // data consecutive check
        EXPECT_EQ(current_value - previous_int, 1);
        previous_int = current_value;
    }

    // total number of element check
    EXPECT_EQ(int_msg_count, kMessageNum);
}

}  // namespace cris::core
