#include "cris/core/timer/timer.h"

#include <glog/logging.h>

namespace cris::core {

cr_timestamp_nsec_t GetSystemTimestampNsec() {
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::steady_clock;
    return static_cast<cr_timestamp_nsec_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

std::atomic<size_t> TimerSection::collector_index_count{0};

TimerReport::TimerReport(const std::string &name, uint64_t hits, cr_timestamp_nsec_t total_duration)
    : mSectionName(name)
    , mHits(hits)
    , mTotalDuration(total_duration) {
}

void TimerReport::AddSubsection(std::unique_ptr<TimerReport> &&subsection) {
    auto subsection_name = subsection->GetSectionName();
    auto result          = mSubsections.emplace(subsection_name, std::move(subsection));
    if (!result.second) {
        LOG(ERROR) << "Failed to insert subsection " << subsection_name << " to section "
                   << GetSectionName();
    }
}

std::string TimerReport::GetSectionName() const {
    return mSectionName;
}

uint64_t TimerReport::GetHits() const {
    return mHits;
}

cr_dutration_nsec_t TimerReport::GetAverageDurationNsec() const {
    return mTotalDuration / mHits;
}

TimerSession::TimerSession(cr_timestamp_nsec_t started_timestamp, size_t collector_index)
    : mStartedTimestamp(started_timestamp)
    , mCollectorIndex(collector_index) {
}

TimerSession::~TimerSession() {
    EndSession();
}

TimerSection* TimerSection::GetMainSection() {
    static TimerSection main_section("main", collector_index_count.fetch_add(1), {});
    return &main_section;
}

TimerSection::TimerSection(const std::string &name, size_t collector_index, CtorPermission)
    : mName(name)
    , mCollectorIndex(collector_index) {
}

#ifdef ENABLE_PROFILING

void TimerSession::EndSession() {
    if (mIsEnded) {
        return;
    }
    mIsEnded      = true;
    auto duration = GetSystemTimestampNsec() - mStartedTimestamp;
    (void)duration;
    // TODO: ReportToCollector(mCollectorIndex, duration);
}

TimerSession TimerSection::StartTimerSession() {
    return TimerSession(GetSystemTimestampNsec(), mCollectorIndex);
}

std::unique_ptr<TimerReport> TimerSection::GetReport(bool recursive) {
    auto report =
        std::make_unique<TimerReport>();  // TODO: =
                                          // GetTotalTimerStats()->GetReport(mCollectorIndex);
    if (recursive) {
        for (auto &&entry : mSubsections) {
            report->AddSubsection(entry.second->GetReport(recursive));
        }
    }
    return report;
}

TimerSection *TimerSection::SubSection(const std::string &name) {
    auto search = mSubsections.find(name);
    if (search != mSubsections.end()) {
        return search->second.get();
    }

    auto insert = mSubsections.emplace(
        name,
        std::make_unique<TimerSection>(name, collector_index_count.fetch_add(1), CtorPermission()));
    return insert.first->second.get();
}

void TimerSection::ReportDurationNsec(cr_timestamp_nsec_t duration) {
    // TODO: ReportToCollector(mCollectorIndex, duration);
}

#else

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

#endif

}  // namespace cris::core
