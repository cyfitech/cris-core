#include "cris/core/timer/timer.h"

#include "gtest/gtest.h"

namespace cris::core {

TEST(TimerTest, Basic) {
    auto *section =
        TimerSection::GetMainSection()->SubSection("section 1")->SubSection("subsection 2");
    ASSERT_NE(section, nullptr);
    [[maybe_unused]] auto timer  = section->StartTimerSession();
    timer.EndSession();
    [[maybe_unused]] auto report = section->GetReport();
}

}  // namespace cris::core
