#ifndef ENABLE_PROFILING

#include <glog/logging.h>

#include <mutex>

#include "cris/core/timer/timer.h"

namespace cris::core {

TimerSection *TimerSection::GetMainSection() {
    static TimerSection main_section("main", 0, {});
    return &main_section;
}

TimerSection::TimerSection(const std::string &name, size_t collector_index, CtorPermission)
    : mName(name)
    , mCollectorIndex(collector_index) {
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

}  // namespace cris::core

#endif
