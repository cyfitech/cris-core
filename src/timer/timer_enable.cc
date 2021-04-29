#ifdef ENABLE_PROFILING

#include <glog/logging.h>

#include <mutex>

#include "cris/core/timer/timer.h"

namespace cris::core {

std::atomic<size_t> TimerSection::collector_index_count{0};

class TimerStatTotal;

class TimerStatCollector {
   public:
    struct CollectorEntry {
        std::atomic<uint64_t> mHitAndTotalDuration{0};
    };

    template<int hit_count_bits = 22, int total_duration_bits = 64 - hit_count_bits>
    struct HitAndTotalDurationType {
        uint64_t hits
            : hit_count_bits
            , total_duration : total_duration_bits;
    };

    static_assert(sizeof(HitAndTotalDurationType<>) == sizeof(uint64_t));

    void Report(size_t index, cr_dutration_nsec_t duration);

    static TimerStatCollector *GetTimerStatsCollector();

   private:
    void Clear();

    static constexpr size_t kRotatingCollectorNum = 3;
    static constexpr size_t kCollectorSize        = 256;

    static std::atomic<size_t>                                   current_collector_id;
    static std::array<TimerStatCollector, kRotatingCollectorNum> rotating_collectors;
    static std::mutex                                            rotating_mutex;

    std::array<CollectorEntry, kCollectorSize> mCollectorEntries;

    // The size of the collector entry is not large enough to hold the
    // timer stats for the entire lifestyle of the process, so the collector
    // needs to be rotated after a while. current_collector_id is the one
    // receiving data currently, (current_collector_id + kRotatingCollectorNum - 1) %
    // kRotatingCollectorNum is the receiver in the last cycle and read-only for the current cycle.
    // (current_collector_id + 1) % kRotatingCollectorNum is the receiver for the next cycle.
    static void RotateCollector();

    friend class TimerStatTotal;
};

class TimerStatTotal {};

std::atomic<size_t> TimerStatCollector::current_collector_id{0};
std::array<TimerStatCollector, TimerStatCollector::kRotatingCollectorNum>
           TimerStatCollector::rotating_collectors;
std::mutex TimerStatCollector::rotating_mutex;

TimerStatCollector *TimerStatCollector::GetTimerStatsCollector() {
    return &rotating_collectors[current_collector_id.load()];
}

void TimerStatCollector::RotateCollector() {
    std::unique_lock<std::mutex> lock(rotating_mutex);
    size_t                       expect_current_rotater_id = current_collector_id.load();
    size_t                       next_rotater_id;
    do {
        next_rotater_id = (expect_current_rotater_id + 1) % kRotatingCollectorNum;
    } while (
        !current_collector_id.compare_exchange_strong(expect_current_rotater_id, next_rotater_id));

    next_rotater_id = (expect_current_rotater_id + 1) % kRotatingCollectorNum;
    rotating_collectors[next_rotater_id].Clear();
}

void TimerStatCollector::Report(size_t index, cr_dutration_nsec_t duration) {
    if (duration < 0) {
        LOG(ERROR) << "duration " << duration << " is smaller than zero, skip it";
        return;
    }

    if (index >= kCollectorSize) {
        LOG(ERROR) << "index " << index << " is out of range";
        return;
    }

    union {
        uint64_t                  raw;
        HitAndTotalDurationType<> stat;
    } data_to_report;

    data_to_report.stat.hits           = 1;
    data_to_report.stat.total_duration = duration;
    mCollectorEntries[index].mHitAndTotalDuration.fetch_add(data_to_report.raw);
}

void TimerStatCollector::Clear() {
    for (auto &&entry : mCollectorEntries) {
        entry.mHitAndTotalDuration.store(0);
    }
}

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

TimerSection *TimerSection::GetMainSection() {
    static TimerSection main_section("main", collector_index_count.fetch_add(1), {});
    return &main_section;
}

TimerSection::TimerSection(const std::string &name, size_t collector_index, CtorPermission)
    : mName(name)
    , mCollectorIndex(collector_index) {
}

void TimerSession::EndSession() {
    if (mIsEnded) {
        return;
    }
    mIsEnded      = true;
    auto duration = GetSystemTimestampNsec() - mStartedTimestamp;
    (void)duration;
    TimerStatCollector::GetTimerStatsCollector()->Report(mCollectorIndex, duration);
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
    TimerStatCollector::GetTimerStatsCollector()->Report(mCollectorIndex, duration);
}

}  // namespace cris::core

#endif  // ENABLE_PROFILING
