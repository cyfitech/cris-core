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
    void SetUp() override { std::filesystem::create_directories(test_temp_dir_); }

    void TearDown() override { std::filesystem::remove_all(test_temp_dir_); }

    std::filesystem::path test_temp_dir_{
        std::filesystem::temp_directory_path() / (std::string("CRPauseTestTmpDir.") + std::to_string(getpid()))};

    const std::string exchange_symbol_str = std::string("TEST_HUOBI") + "__" + std::string("TEST_PERPETUAL");
    const std::string filename            = exchange_symbol_str + "_subid_0.ldb.d";

    const std::filesystem::path record_file_path = test_temp_dir_ / filename;

    static constexpr std::size_t kMessageNum = 10;
};

TEST_F(RecorderPauseTest, RecorderPauseTest) {
    std::filesystem::create_directories(record_file_path);
    auto record_file = std::make_unique<RecordFile>(record_file_path);

    // repeatedly open and close the DB
    for (std::size_t i = 0; i < kMessageNum; ++i) {
        if (i % 2 == 0) {
            record_file->CloseDB();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            record_file->OpenDB();
        }
        record_file->Write(std::to_string(i));
    }

    int previous_int  = -1;
    int int_msg_count = 0;

    for (auto itr = record_file->Iterate(); itr.Valid(); itr.Next()) {
        int current_value = std::stoi(itr.Get().second);

        // first element expect to be zero
        if (int_msg_count == 0) {
            EXPECT_EQ(current_value, 0);
        }

        // data consecutive check
        EXPECT_EQ(current_value - previous_int, 1);
        previous_int = current_value;
        int_msg_count++;
    }

    // total number of element check
    EXPECT_EQ(int_msg_count, kMessageNum);
}

}  // namespace cris::core
