#include "cris/core/timer/timer.h"

#include <benchmark/benchmark.h>

#include <chrono>

namespace cris::core {

static void BM_TimerSessionOverhead(benchmark::State& state) {
    auto* test_section = TimerSection::GetMainSection()->SubSection("benchmark")->SubSection(__func__);
    for ([[maybe_unused]] const auto s : state) {
        auto session = test_section->StartTimerSession();
    }
}

static void BM_TimerReportShortDuration(benchmark::State& state) {
    auto* test_section = TimerSection::GetMainSection()->SubSection("benchmark")->SubSection(__func__);
    for ([[maybe_unused]] const auto s : state) {
        test_section->ReportDurationNsec(1);
    }
}

static void BM_TimerReportMediumDuration(benchmark::State& state) {
    auto* test_section = TimerSection::GetMainSection()->SubSection("benchmark")->SubSection(__func__);
    for ([[maybe_unused]] const auto s : state) {
        test_section->ReportDuration(std::chrono::microseconds(5));
    }
}

static void BM_TimerReportLongDuration(benchmark::State& state) {
    auto* test_section = TimerSection::GetMainSection()->SubSection("benchmark")->SubSection(__func__);
    for ([[maybe_unused]] const auto s : state) {
        test_section->ReportDuration(std::chrono::milliseconds(10));
    }
}

BENCHMARK(BM_TimerSessionOverhead)->ThreadRange(1, 4);
BENCHMARK(BM_TimerReportShortDuration)->ThreadRange(1, 4);
BENCHMARK(BM_TimerReportMediumDuration)->ThreadRange(1, 4);
BENCHMARK(BM_TimerReportLongDuration)->ThreadRange(1, 4);

}  // namespace cris::core
