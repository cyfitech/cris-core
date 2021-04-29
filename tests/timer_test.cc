#include "cris/core/timer/timer.h"

#include <chrono>
#include <thread>

#include "gtest/gtest.h"

namespace cris::core {

template<class T1, class T2>
void ExpectNear(const T1 &v1, const T2 &v2, float error = 0.05) {
    EXPECT_LT((1 - error) * v2, v1);
    EXPECT_LT(v1, (1 + error) * v2);
}

TEST(TimerTest, Basic) {
    std::string section1_name = "section 1";
    std::string section2_name = "subsection 2";
    std::string section3_name = "section 3";
    auto *      section1      = TimerSection::GetMainSection()->SubSection(section1_name);
    ASSERT_NE(section1, nullptr);
    auto *section2 = section1->SubSection(section2_name);
    ASSERT_NE(section2, nullptr);
    auto *section3 = TimerSection::GetMainSection()->SubSection(section3_name);
    ASSERT_NE(section3, nullptr);

    constexpr auto kTestHits            = 100;
    constexpr auto kTestSessionDuration = std::chrono::milliseconds(10);

    auto timer1 = section1->StartTimerSession();

    std::atomic<bool> stop_flushing{false};
    auto              crazy_flushing_thread = std::thread([&stop_flushing]() {
        while (!stop_flushing.load()) {
            TimerSection::FlushCollectedStats();
        }
    });

    for (size_t i = 0; i < kTestHits; ++i) {
        auto timer2 = section2->StartTimerSession();
        std::this_thread::sleep_for(kTestSessionDuration);
    }

    timer1.EndSession();

    section3->ReportDuration(kTestSessionDuration);

    // make sure flush after end-of-session
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop_flushing.store(true);
    crazy_flushing_thread.join();

    auto report1 = section1->GetReport(true);
    auto report3 = section3->GetReport(false);

#ifdef ENABLE_PROFILING
    auto &report2 = report1->mSubsections[section2_name];
    EXPECT_EQ(report1->GetHits(), 1);
    ExpectNear(
        report1->GetAverageDurationNsec(),
        kTestHits *
            std::chrono::duration_cast<std::chrono::nanoseconds>(kTestSessionDuration).count());

    EXPECT_EQ(report2->GetHits(), kTestHits);
    ExpectNear(report2->GetAverageDurationNsec(),
               std::chrono::duration_cast<std::chrono::nanoseconds>(kTestSessionDuration).count());

    EXPECT_EQ(report3->GetHits(), 1);
    EXPECT_EQ(report3->GetAverageDurationNsec(),
              std::chrono::duration_cast<std::chrono::nanoseconds>(kTestSessionDuration).count());
#endif  // ENABLE_PROFILING
}

}  // namespace cris::core
