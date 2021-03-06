#ifndef ENABLE_PROFILING

#include "cris/core/timer/timer.h"
#include "cris/core/utils/logging.h"

#include <cstdint>
#include <mutex>

namespace cris::core {

void TimerReport::AddSubsection(std::unique_ptr<TimerReport>&&) {
}

std::string TimerReport::GetSectionName() const {
    return "";
}

unsigned long long TimerReport::GetTotalHits() const {
    return 0;
}

double TimerReport::GetFreq() const {
    return 0;
}

cr_duration_nsec_t TimerReport::GetAverageDurationNsec() const {
    return 0;
}

cr_duration_nsec_t TimerReport::GetPercentileDurationNsec(int /* percent */) const {
    return 0;
}

void TimerReport::PrintToLog(unsigned /* indent_level */) const {
}

TimerSection* TimerSection::GetMainSection() {
    static TimerSection main_section("main", 0, {});
    return &main_section;
}

TimerSection::TimerSection(const std::string& name, std::size_t collector_index, CtorPermission)
    : name_(name)
    , collector_index_(collector_index) {
}

TimerSession::TimerSession(cr_timestamp_nsec_t, std::size_t) {
}

TimerSession::~TimerSession() {
}

void TimerSession::EndSession() {
}

TimerSession TimerSection::StartTimerSession() {
    return {};
}

std::unique_ptr<TimerReport> TimerSection::GetReport(bool /* recursive */) {
    return std::make_unique<TimerReport>();
}

std::unique_ptr<TimerReport> TimerSection::GetAllReports(bool /* clear */) {
    return std::make_unique<TimerReport>();
}

TimerSection* TimerSection::SubSection(const std::string& /* name */) {
    return this;
}

void TimerSection::ReportDurationNsec(cr_timestamp_nsec_t) {
}

}  // namespace cris::core

#endif
