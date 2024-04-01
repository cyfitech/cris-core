#include "cris/core/timer/timer.h"

#include "gtest/gtest.h"

#include <chrono>
#include <cstddef>
#include <thread>

namespace cris::core {

TEST(TimerFullTest, TimerIsFull) {
    constexpr std::size_t kTestSectionNum = 300000;
    EXPECT_GT(kTestSectionNum, TimerSection::kMaxCapacity);
    auto* main_section = TimerSection::GetMainSection();
    for (std::size_t i = 0; i < kTestSectionNum; ++i) {
        auto* section = main_section->SubSection(std::to_string(i));
        ASSERT_NE(section, nullptr);
        auto session = section->StartTimerSession();
    }
    TimerSection::GetAllReports(/*clear = */ true)->PrintToLog();
}

}  // namespace cris::core
