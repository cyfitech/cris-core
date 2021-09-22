#ifndef ENABLE_PROFILING

#include "cris/core/timer/timer.h"

#include <glog/logging.h>

#include <mutex>

namespace cris::core {

void TimerReport::AddSubsection(std::unique_ptr<TimerReport> &&) {
}

std::string TimerReport::GetSectionName() const {
    return "";
}

uint64_t TimerReport::GetTotalHits() const {
    return 0;
}

double TimerReport::GetFreq() const {
    return 0;
}

cr_dutration_nsec_t TimerReport::GetAverageDurationNsec() const {
    return 0;
}

cr_dutration_nsec_t TimerReport::GetPercentileDurationNsec(int percent) const {
    return 0;
}

void TimerReport::PrintToLog(int indent_level) const {
}

TimerSection *TimerSection::GetMainSection() {
    static TimerSection main_section("main", 0, {});
    return &main_section;
}

TimerSection::TimerSection(const std::string &name, size_t collector_index, CtorPermission)
    : mName(name)
    , mCollectorIndex(collector_index) {
}

TimerSession::TimerSession(cr_timestamp_nsec_t, size_t) {
}

TimerSession::~TimerSession() {
}

void TimerSession::EndSession() {
}

TimerSession TimerSection::StartTimerSession() {
    return {};
}

std::unique_ptr<TimerReport> TimerSection::GetReport(bool recursive) {
    return {};
}

TimerSection *TimerSection::SubSection(const std::string &name) {
    return this;
}

void TimerSection::ReportDurationNsec(cr_timestamp_nsec_t duration) {
}

void TimerSection::FlushCollectedStats() {
}

}  // namespace cris::core

#endif
