#include "cris/core/timer/timer.h"

#include "gtest/gtest.h"

namespace cris::core {

TEST(TimerTest, Basic) {
    auto *section = TimerSection::GetMainSection();
    ASSERT_NE(section, nullptr);
    [[maybe_unused]] auto timer  = section->StartTimerSession();
    [[maybe_unused]] auto report = section->GetReport();
}

}  // namespace cris::core
