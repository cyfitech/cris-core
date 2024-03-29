#include "cris/core/timer/timer.h"

#include "gtest/gtest.h"

#include <chrono>
#include <thread>

namespace cris::core {

template<class T1, class T2>
void ExpectNear(const T1& v1, const T2& v2, double error) {
    const auto d1 = static_cast<double>(v1);
    const auto d2 = static_cast<double>(v2);
    EXPECT_LT((1 - error) * d2, d1);
    EXPECT_LT(d1, (1 + error) * d2);
}

TEST(TimerTest, Basic) {
    std::string section1_name = "section 1";
    std::string section2_name = "subsection 2";
    std::string section3_name = "section 3";
    auto*       section1      = TimerSection::GetMainSection()->SubSection(section1_name);
    ASSERT_NE(section1, nullptr);
    auto* section2 = section1->SubSection(section2_name);
    ASSERT_NE(section2, nullptr);
    auto* section3 = TimerSection::GetMainSection()->SubSection(section3_name);
    ASSERT_NE(section3, nullptr);

    constexpr auto kTestHits            = 100;
    constexpr auto kTestSessionDuration = std::chrono::milliseconds(10);

    const auto section1_start = std::chrono::steady_clock::now();
    auto       timer1         = section1->StartTimerSession();

    for (std::size_t i = 0; i < kTestHits; ++i) {
        auto timer2 = section2->StartTimerSession();
        std::this_thread::sleep_for(kTestSessionDuration);

        // Sessions are movable.
        auto new_timer2 = std::move(timer2);
    }

    timer1.EndSession();
    const auto section1_end = std::chrono::steady_clock::now();

    [[maybe_unused]] const auto section1_duration_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(section1_end - section1_start).count();

    ASSERT_GT(kTestHits, 0);
    [[maybe_unused]] const auto section2_avg_duration_ns = section1_duration_ns / kTestHits;

    section3->ReportDuration(kTestSessionDuration);

    auto report1 = section1->GetReport(true);
    auto report3 = section3->GetReport(false);

#ifdef ENABLE_PROFILING
    auto& report2 = report1->subsections_[section2_name];
    EXPECT_EQ(report1->GetTotalHits(), 1);
    ExpectNear(report1->GetAverageDurationNsec(), section1_duration_ns, /* error = */ 0.05);

    EXPECT_EQ(report2->GetTotalHits(), kTestHits);
    ExpectNear(report2->GetAverageDurationNsec(), section2_avg_duration_ns, /* error = */ 0.05);

    EXPECT_EQ(report3->GetTotalHits(), 1);
    EXPECT_EQ(
        report3->GetAverageDurationNsec(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(kTestSessionDuration).count());
#endif  // ENABLE_PROFILING

    TimerSection::GetAllReports(/*clear = */ true)->PrintToLog();

    EXPECT_EQ(section1->GetReport(false)->GetTotalHits(), 0);
}

TEST(TimerTest, PercentileTest) {
    auto* section = TimerSection::GetMainSection()->SubSection("pecentile test");
    ASSERT_NE(section, nullptr);

    {
        auto report = section->GetReport();
        EXPECT_EQ(report->GetPercentileDurationNsec(-10), 0);
        EXPECT_EQ(report->GetPercentileDurationNsec(0), 0);
        EXPECT_EQ(report->GetPercentileDurationNsec(10), 0);
        EXPECT_EQ(report->GetPercentileDurationNsec(50), 0);
        EXPECT_EQ(report->GetPercentileDurationNsec(100), 0);
        EXPECT_EQ(report->GetPercentileDurationNsec(200), 0);
    }

    // TODO (chenhao.yalier@gmail.com): This part depends on the specific bucketizing setting,
    // which is not great. Needs to find a setting-indepedent way to test
    //
    // Insert 3 records for first 10 duration buckets
    for (std::size_t i = 0; i < 10; ++i) {
        section->ReportDuration(std::chrono::microseconds(10 * (1 << i) - 1));
        section->ReportDuration(std::chrono::microseconds(10 * (1 << i) - 2));
        section->ReportDuration(std::chrono::microseconds(10 * (1 << i) - 3));
    }

    {
        auto report = section->GetReport();
        EXPECT_EQ(report->GetPercentileDurationNsec(-10), 0);
        EXPECT_EQ(report->GetPercentileDurationNsec(0), 0);
#ifdef ENABLE_PROFILING
        // TODO (chenhao.yalier@gmail.com): This part depends on the specific bucketizing setting,
        // which is not great. Needs to find a setting-indepedent way to test
        //
        // Expected value will be the average of a specific bucket
        EXPECT_EQ(report->GetPercentileDurationNsec(10), (10 * (1 << 0) - 2) * 1000);
        EXPECT_EQ(report->GetPercentileDurationNsec(20), (10 * (1 << 1) - 2) * 1000);
        EXPECT_EQ(report->GetPercentileDurationNsec(30), (10 * (1 << 2) - 2) * 1000);
        EXPECT_EQ(report->GetPercentileDurationNsec(40), (10 * (1 << 3) - 2) * 1000);
        EXPECT_EQ(report->GetPercentileDurationNsec(50), (10 * (1 << 4) - 2) * 1000);
        EXPECT_EQ(report->GetPercentileDurationNsec(60), (10 * (1 << 5) - 2) * 1000);
        EXPECT_EQ(report->GetPercentileDurationNsec(70), (10 * (1 << 6) - 2) * 1000);
        EXPECT_EQ(report->GetPercentileDurationNsec(80), (10 * (1 << 7) - 2) * 1000);
        EXPECT_EQ(report->GetPercentileDurationNsec(90), (10 * (1 << 8) - 2) * 1000);
        // 100p always provides the max value
        EXPECT_EQ(report->GetPercentileDurationNsec(100), (10 * (1 << 9) - 1) * 1000);
#endif  // ENABLE_PROFILING
        EXPECT_EQ(report->GetPercentileDurationNsec(200), report->GetPercentileDurationNsec(100));
    }
}

}  // namespace cris::core
