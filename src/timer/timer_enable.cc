#ifdef ENABLE_PROFILING

#include <glog/logging.h>

#include <cstring>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <thread>

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
        uint64_t hits : hit_count_bits;
        uint64_t total_duration_ns : total_duration_bits;
    };

    static_assert(sizeof(HitAndTotalDurationType<>) == sizeof(uint64_t));

    void Report(size_t index, cr_dutration_nsec_t duration);

    static TimerStatCollector *GetTimerStatsCollector();

    // The size of the collector entry is not large enough to hold the
    // timer stats for the entire lifestyle of the process, so the collector
    // needs to be rotated after a while. current_collector_id is the one
    // receiving data currently, (current_collector_id + kRotatingCollectorNum - 1) %
    // kRotatingCollectorNum is the receiver in the last cycle and read-only for the current cycle.
    // (current_collector_id + 1) % kRotatingCollectorNum is the receiver for the next cycle.
    static void RotateCollector();

   private:
    void Clear();

    static constexpr size_t kRotatingCollectorNum = 3;
    static constexpr size_t kCollectorSize        = 256;

    static std::atomic<size_t>                                   current_collector_id;
    static std::array<TimerStatCollector, kRotatingCollectorNum> rotating_collectors;
    static std::mutex                                            rotating_mutex;

    std::array<CollectorEntry, kCollectorSize> mCollectorEntries;

    friend class TimerStatTotal;
};

class TimerStatTotal {
   public:
    struct StatToalEntry {
        uint64_t hits;
        uint64_t total_duration_ns;

        void Merge(const TimerStatCollector::CollectorEntry &entry);
    };

    void Merge(const TimerStatCollector &collector);

    void Clear();

    std::shared_lock<std::shared_mutex> LockForRead();

    std::unique_ptr<TimerReport> GetReport(const std::string &section_name,
                                           size_t             entry_index) const;

    static TimerStatTotal *GetTimerStatsTotal();

   private:
    static constexpr auto kCollectorSize = TimerStatCollector::kCollectorSize;

    TimerStatTotal();

    cr_timestamp_nsec_t                       mLastClearTime;
    cr_timestamp_nsec_t                       mLastMergeTime;
    std::shared_mutex                         mStatTotalMutex;
    std::array<StatToalEntry, kCollectorSize> mStatEntries;
};

class TimerStatRotater {
   public:
    TimerStatRotater();

    ~TimerStatRotater();

   private:
    void RotateWorker();

    std::thread             mThread;
    std::atomic<bool>       mShutdownFlag{false};
    std::condition_variable mShutdownCV;

    // the collector use 22 bits for hits and 42 bits for nsec
    // so that it can record up to 13981 Hz for 5 min, or total
    // duration up to 73 min. 5-min rotating period is safe enough
    // in our use case.
    static constexpr auto kRotatePeriod = std::chrono::minutes(5);
};

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

    // After exiting the while loop, next_rotater_id become the current, and
    // expect_current_rotater_id is the previous collector

    auto previous_rotater_id  = expect_current_rotater_id;
    expect_current_rotater_id = next_rotater_id;
    next_rotater_id           = (next_rotater_id + 1) % kRotatingCollectorNum;

    rotating_collectors[next_rotater_id].Clear();

    // Make sure the data reported to the last collector are arrived
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TimerStatTotal::GetTimerStatsTotal()->Merge(rotating_collectors[previous_rotater_id]);
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
    data_to_report.stat.total_duration_ns = duration;
    mCollectorEntries[index].mHitAndTotalDuration.fetch_add(data_to_report.raw);
}

void TimerStatCollector::Clear() {
    for (auto &&entry : mCollectorEntries) {
        entry.mHitAndTotalDuration.store(0);
    }
}

TimerStatTotal::TimerStatTotal()
    : mLastClearTime(GetSystemTimestampNsec())
    , mLastMergeTime(mLastClearTime) {
}

void TimerStatTotal::Merge(const TimerStatCollector &collector) {
    std::unique_lock lock(mStatTotalMutex);
    for (size_t i = 0; i < kCollectorSize; ++i) {
        mStatEntries[i].Merge(collector.mCollectorEntries[i]);
    }
    mLastMergeTime = GetSystemTimestampNsec();
}

void TimerStatTotal::Clear() {
    std::unique_lock lock(mStatTotalMutex);
    std::memset(mStatEntries.data(), 0, sizeof(mStatEntries));
    mLastClearTime = GetSystemTimestampNsec();
    mLastMergeTime = mLastClearTime;
}

std::shared_lock<std::shared_mutex> TimerStatTotal::LockForRead() {
    return std::shared_lock(mStatTotalMutex);
}

std::unique_ptr<TimerReport> TimerStatTotal::GetReport(const std::string &section_name,
                                                       size_t             entry_index) const {
    auto  report                = std::make_unique<TimerReport>();
    auto &entry                 = mStatEntries[entry_index];
    report->mSectionName        = section_name;
    report->mHits               = entry.hits;
    report->mSessionDurationSum = entry.total_duration_ns;
    report->mTimingDuration     = mLastMergeTime - mLastClearTime;
    return report;
}

TimerStatTotal *TimerStatTotal::GetTimerStatsTotal() {
    static TimerStatTotal total;
    return &total;
}

void TimerStatTotal::StatToalEntry::Merge(const TimerStatCollector::CollectorEntry &entry) {
    union {
        uint64_t                                      raw;
        TimerStatCollector::HitAndTotalDurationType<> stat;
    } data_to_merge;

    data_to_merge.raw = entry.mHitAndTotalDuration.load();
    hits += data_to_merge.stat.hits;
    total_duration_ns += data_to_merge.stat.total_duration_ns;
}

TimerStatRotater::TimerStatRotater() : mThread(std::bind(&TimerStatRotater::RotateWorker, this)) {
}

TimerStatRotater::~TimerStatRotater() {
    mShutdownFlag.store(true);
    mShutdownCV.notify_all();
    if (mThread.joinable()) {
        mThread.join();
    }
}

void TimerStatRotater::RotateWorker() {
    std::mutex mutex;
    while (!mShutdownFlag.load()) {
        std::unique_lock lock(mutex);
        auto             is_shutdown =
            mShutdownCV.wait_for(lock, kRotatePeriod, [this]() { return mShutdownFlag.load(); });
        if (is_shutdown) {
            break;
        }
        TimerSection::FlushCollectedStats();
    }
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

double TimerReport::GetFreq() const {
    using std::chrono::duration;
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    return mHits * 1.0 / duration_cast<duration<double>>(nanoseconds(mTimingDuration)).count();
}

cr_dutration_nsec_t TimerReport::GetAverageDurationNsec() const {
    return mSessionDurationSum / mHits;
}

void TimerReport::PrintToLog(int indent_level) const {
    using std::chrono::duration;
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    const std::string indent(4 * indent_level, ' ');
    LOG(INFO) << indent << "Section '" << GetSectionName() << "':";
    if (GetHits() > 0) {
        LOG(INFO) << indent << "    hits    : " << GetHits() << " times, freq: " << GetFreq()
                  << " Hz";
        LOG(INFO) << indent << "    avg time: "
                  << duration_cast<duration<double, std::milli>>(
                         nanoseconds(GetAverageDurationNsec()))
                         .count()
                  << " ms";
        LOG(INFO);
    }
    if (!mSubsections.empty()) {
        LOG(INFO) << indent << "Subsections:";
        LOG(INFO);
        for (auto &&subsection : mSubsections) {
            subsection.second->PrintToLog(indent_level + 1);
        }
    }
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
    static TimerStatRotater rotater;
    return &main_section;
}

void TimerSection::FlushCollectedStats() {
    TimerStatCollector::RotateCollector();
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
    TimerStatCollector::GetTimerStatsCollector()->Report(mCollectorIndex, duration);
}

TimerSession TimerSection::StartTimerSession() {
    return TimerSession(GetSystemTimestampNsec(), mCollectorIndex);
}

std::unique_ptr<TimerReport> TimerSection::GetReport(bool recursive) {
    auto *total     = TimerStatTotal::GetTimerStatsTotal();
    auto  read_lock = total->LockForRead();
    auto  report    = total->GetReport(mName, mCollectorIndex);
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
