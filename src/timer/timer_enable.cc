#ifdef ENABLE_PROFILING

#include "cris/core/logging.h"
#include "cris/core/timer/timer.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <mutex>
#include <shared_mutex>
#include <thread>

namespace cris::core {

std::atomic<size_t> TimerSection::collector_index_count{0};

class TimerStatTotal;

// Number of duration buckets in each Collector/Total Entry
static constexpr size_t kTimerEntryBucketNum = 32;

// Calculate the upper (exclusive) limit for each bucket.
// The bucket range grows exponentially. The range of the
// first bucket is 0 to base_nsec.
static constexpr cr_duration_nsec_t UpperNsecOfBucket(
    size_t             idx,
    cr_duration_nsec_t base_nsec = 10 * std::ratio_divide<std::micro, std::nano>::num /* 10 us */) {
    // The upper limit of the last bucket should be INT_MAX to cover everything
    if (idx >= kTimerEntryBucketNum - 1) {
        return std::numeric_limits<cr_duration_nsec_t>::max();
    }
    auto result = base_nsec;
    for (size_t i = 0; i < idx; ++i) {
        result *= 2;
    }
    return result;
}

template<size_t... idx>
static constexpr auto GenerateBucketUpperNsec(std::integer_sequence<size_t, idx...>)
    -> std::array<cr_duration_nsec_t, sizeof...(idx)> {
    return {UpperNsecOfBucket(idx)...};
}

// Upper limit nsec (exclusive) of duration buckets of each timer entry
static constexpr auto kBucketUpperNsec =
    GenerateBucketUpperNsec(std::make_integer_sequence<size_t, kTimerEntryBucketNum>{});

static_assert(kBucketUpperNsec[4] == 160000);
static_assert(kBucketUpperNsec[6] == 640000);
static_assert(kBucketUpperNsec[kTimerEntryBucketNum - 1] > 10000 * std::nano::den);

class TimerStatCollector {
   public:
    struct CollectorEntryBucket {
        std::atomic<std::uint64_t> hit_and_total_duration_{0};
    };

    struct CollectorEntry {
        std::array<CollectorEntryBucket, kTimerEntryBucketNum> duration_buckets_{};
    };

    template<int hit_count_bits = 22, int total_duration_bits = 64 - hit_count_bits>
    struct HitAndTotalDurationType {
        std::uint64_t hits : hit_count_bits;
        std::uint64_t total_duration_ns : total_duration_bits;
    };

    static_assert(sizeof(HitAndTotalDurationType<>) == sizeof(std::uint64_t));

    void Report(size_t index, cr_duration_nsec_t duration);

    static TimerStatCollector* GetTimerStatsCollector();

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
    static constexpr size_t kCollectorSize        = 65536;

    static std::atomic<size_t>                                   current_collector_id;
    static std::array<TimerStatCollector, kRotatingCollectorNum> rotating_collectors;
    static std::mutex                                            rotating_mutex;

    std::array<CollectorEntry, kCollectorSize> collector_entries_;

    friend class TimerStatTotal;
};

class TimerStatTotal {
   public:
    struct StatTotalEntryBucket {
        std::uint64_t hits;
        std::uint64_t total_duration_ns;
    };

    struct StatTotalEntry {
        std::array<StatTotalEntryBucket, kTimerEntryBucketNum> duration_buckets_;

        void Merge(const TimerStatCollector::CollectorEntry& entry);
    };

    void Merge(const TimerStatCollector& collector);

    void Clear();

    std::shared_lock<std::shared_mutex> LockForRead();

    // call with LockForRead
    std::unique_ptr<TimerReport> GetReport(const std::string& section_name, size_t entry_index) const;

    static TimerStatTotal* GetTimerStatsTotal();

   private:
    static constexpr auto kCollectorSize = TimerStatCollector::kCollectorSize;

    TimerStatTotal();

    cr_timestamp_nsec_t                        last_clear_time_;
    cr_timestamp_nsec_t                        last_merge_time_;
    std::shared_mutex                          stat_total_mutex_;
    std::array<StatTotalEntry, kCollectorSize> stat_entries_;
};

class TimerStatRotater {
   public:
    TimerStatRotater();

    ~TimerStatRotater();

   private:
    void RotateWorker();

    std::thread             thread_;
    std::atomic<bool>       shutdown_flag_{false};
    std::condition_variable shutdown_cv_;

    // the collector use 22 bits for hits and 42 bits for nsec
    // so that it can record up to 13981 Hz for 5 min, or total
    // duration up to 73 min. 5-min rotating period is safe enough
    // in our use case.
    static constexpr auto kRotatePeriod = std::chrono::minutes(5);
};

std::atomic<size_t>                                                       TimerStatCollector::current_collector_id{0};
std::array<TimerStatCollector, TimerStatCollector::kRotatingCollectorNum> TimerStatCollector::rotating_collectors;
std::mutex                                                                TimerStatCollector::rotating_mutex;

TimerStatCollector* TimerStatCollector::GetTimerStatsCollector() {
    return &rotating_collectors[current_collector_id.load()];
}

void TimerStatCollector::RotateCollector() {
    std::unique_lock<std::mutex> lock(rotating_mutex);
    size_t                       expect_current_rotater_id = current_collector_id.load();
    size_t                       next_rotater_id;
    do {
        next_rotater_id = (expect_current_rotater_id + 1) % kRotatingCollectorNum;
    } while (!current_collector_id.compare_exchange_strong(expect_current_rotater_id, next_rotater_id));

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

void TimerStatCollector::Report(size_t index, cr_duration_nsec_t duration) {
    if (duration < 0) {
        LOG(ERROR) << "duration " << duration << " is smaller than zero, skip it";
        return;
    }

    if (index >= kCollectorSize) {
        LOG(ERROR) << "index " << index << " is out of range";
        return;
    }

    union {
        std::uint64_t             raw;
        HitAndTotalDurationType<> stat;
    } data_to_report;

    // Linear scan is better than binary search here, since shorter
    // session should react faster, while it is ok for long sessions
    // to be a little slower
    size_t bucket_idx = 0;
    for (size_t i = 0; i < kTimerEntryBucketNum; ++i) {
        bucket_idx = i;
        if (duration < kBucketUpperNsec[i]) {
            break;
        }
    }

    data_to_report.stat.hits              = 1;
    data_to_report.stat.total_duration_ns = duration;
    collector_entries_[index].duration_buckets_[bucket_idx].hit_and_total_duration_.fetch_add(data_to_report.raw);
}

void TimerStatCollector::Clear() {
    for (auto&& entry : collector_entries_) {
        for (auto&& bucket : entry.duration_buckets_) {
            bucket.hit_and_total_duration_.store(0);
        }
    }
}

TimerStatTotal::TimerStatTotal() : last_clear_time_(GetSystemTimestampNsec()), last_merge_time_(last_clear_time_) {
}

void TimerStatTotal::Merge(const TimerStatCollector& collector) {
    std::unique_lock lock(stat_total_mutex_);
    for (size_t i = 0; i < kCollectorSize; ++i) {
        stat_entries_[i].Merge(collector.collector_entries_[i]);
    }
    last_merge_time_ = GetSystemTimestampNsec();
}

void TimerStatTotal::Clear() {
    std::unique_lock lock(stat_total_mutex_);
    std::memset(stat_entries_.data(), 0, sizeof(stat_entries_));
    last_clear_time_ = GetSystemTimestampNsec();
    last_merge_time_ = last_clear_time_;
}

std::shared_lock<std::shared_mutex> TimerStatTotal::LockForRead() {
    return std::shared_lock(stat_total_mutex_);
}

std::unique_ptr<TimerReport> TimerStatTotal::GetReport(const std::string& section_name, size_t entry_index) const {
    auto  report             = std::make_unique<TimerReport>();
    auto& entry              = stat_entries_[entry_index];
    report->section_name_    = section_name;
    report->timing_duration_ = last_merge_time_ - last_clear_time_;
    for (size_t i = 0; i < kTimerEntryBucketNum; ++i) {
        report->report_buckets_.emplace_back(TimerReport::TimerReportBucket{
            .hits_                 = entry.duration_buckets_[i].hits,
            .session_duration_sum_ = static_cast<cr_duration_nsec_t>(entry.duration_buckets_[i].total_duration_ns),
        });
    }
    return report;
}

TimerStatTotal* TimerStatTotal::GetTimerStatsTotal() {
    static TimerStatTotal total;
    return &total;
}

void TimerStatTotal::StatTotalEntry::Merge(const TimerStatCollector::CollectorEntry& entry) {
    union {
        std::uint64_t                                 raw;
        TimerStatCollector::HitAndTotalDurationType<> stat;
    } data_to_merge;

    for (size_t i = 0; i < kTimerEntryBucketNum; ++i) {
        data_to_merge.raw = entry.duration_buckets_[i].hit_and_total_duration_.load();
        auto& bucket      = duration_buckets_[i];
        bucket.hits += data_to_merge.stat.hits;
        bucket.total_duration_ns += data_to_merge.stat.total_duration_ns;
    }
}

TimerStatRotater::TimerStatRotater() : thread_(std::bind(&TimerStatRotater::RotateWorker, this)) {
}

TimerStatRotater::~TimerStatRotater() {
    shutdown_flag_.store(true);
    shutdown_cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void TimerStatRotater::RotateWorker() {
    std::mutex mutex;
    while (!shutdown_flag_.load()) {
        std::unique_lock lock(mutex);
        auto is_shutdown = shutdown_cv_.wait_for(lock, kRotatePeriod, [this]() { return shutdown_flag_.load(); });
        if (is_shutdown) {
            break;
        }
        TimerSection::FlushCollectedStats();
    }
}

void TimerReport::AddSubsection(std::unique_ptr<TimerReport>&& subsection) {
    auto subsection_name = subsection->GetSectionName();
    auto result          = subsections_.emplace(subsection_name, std::move(subsection));
    if (!result.second) {
        LOG(ERROR) << "Failed to insert subsection " << subsection_name << " to section " << GetSectionName();
    }
}

std::string TimerReport::GetSectionName() const {
    return section_name_;
}

std::uint64_t TimerReport::GetTotalHits() const {
    std::uint64_t total_hits = 0;
    for (const auto& bucket : report_buckets_) {
        total_hits += bucket.hits_;
    }
    return total_hits;
}

double TimerReport::GetFreq() const {
    using std::chrono::duration;
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    return static_cast<double>(GetTotalHits()) / duration_cast<duration<double>>(nanoseconds(timing_duration_)).count();
}

cr_duration_nsec_t TimerReport::GetAverageDurationNsec() const {
    std::uint64_t      total_hits         = 0;
    cr_duration_nsec_t total_duration_sum = 0;
    for (const auto& bucket : report_buckets_) {
        total_hits += bucket.hits_;
        total_duration_sum += bucket.session_duration_sum_;
    }
    return total_duration_sum / total_hits;
}

cr_duration_nsec_t TimerReport::GetPercentileDurationNsec(int percent) const {
    if (percent < 0) [[unlikely]] {
        LOG(ERROR) << __func__ << ": percent less than 0: " << percent;
        percent = 0;
    }
    if (percent > 100) [[unlikely]] {
        LOG(ERROR) << __func__ << ": percent greater than 100: " << percent;
        percent = 100;
    }

    const auto total_hits = GetTotalHits();
    const auto target_hits = static_cast<unsigned long long>(
        std::round(static_cast<double>(total_hits) * static_cast<double>(percent) / 100.));

    if (target_hits == 0) {
        return 0;
    }

    unsigned long long current_hits = 0;
    for (const auto& bucket : report_buckets_) {
        current_hits += bucket.hits_;
        if (current_hits >= target_hits) {
            return bucket.session_duration_sum_ / bucket.hits_;
        }
    }

    LOG(ERROR) << __func__ << ": NEVER REACH HERE!!!!";
    return 0;
}

void TimerReport::PrintToLog(int indent_level) const {
    using std::chrono::duration;
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    const std::string indent(4 * indent_level, ' ');
    LOG(WARNING) << indent << "Section '" << GetSectionName() << "':";
    if (GetTotalHits() > 0) {
        LOG(WARNING) << indent << "    hits    : " << GetTotalHits() << " times, freq: " << GetFreq() << " Hz";
        LOG(WARNING) << indent << "    avg time: " << std::fixed << std::setprecision(3)
                     << duration_cast<duration<double, std::micro>>(nanoseconds(GetAverageDurationNsec())).count()
                     << " us";
        auto print_percentile = [&](int percent) {
            LOG(WARNING)
                << indent << "    " << percent << "p time: " << std::fixed << std::setprecision(3)
                << duration_cast<duration<double, std::micro>>(nanoseconds(GetPercentileDurationNsec(percent))).count()
                << " us";
        };
        print_percentile(50);
        print_percentile(90);
        print_percentile(95);
        print_percentile(99);
        LOG(WARNING);
    }
    if (!subsections_.empty()) {
        LOG(WARNING) << indent << "Subsections:";
        LOG(WARNING);
        for (auto&& subsection : subsections_) {
            subsection.second->PrintToLog(indent_level + 1);
        }
    }
}

TimerSession::TimerSession(cr_timestamp_nsec_t started_timestamp, size_t collector_index)
    : started_timestamp_(started_timestamp)
    , collector_index_(collector_index) {
}

TimerSession::~TimerSession() {
    EndSession();
}

TimerSection* TimerSection::GetMainSection() {
    static TimerSection main_section("main", collector_index_count.fetch_add(1), {});

    // Launching rotating before starting profiling
    static TimerStatRotater rotater;

    // Initializing stat total to record the start time of profiling.
    [[maybe_unused]] static TimerStatTotal* total = TimerStatTotal::GetTimerStatsTotal();

    return &main_section;
}

void TimerSection::FlushCollectedStats() {
    TimerStatCollector::RotateCollector();
}

TimerSection::TimerSection(const std::string& name, size_t collector_index, CtorPermission)
    : name_(name)
    , collector_index_(collector_index) {
}

void TimerSession::EndSession() {
    if (is_ended_) {
        return;
    }
    is_ended_     = true;
    auto duration = GetSystemTimestampNsec() - started_timestamp_;
    TimerStatCollector::GetTimerStatsCollector()->Report(collector_index_, duration);
}

TimerSession TimerSection::StartTimerSession() {
    return TimerSession(GetSystemTimestampNsec(), collector_index_);
}

std::unique_ptr<TimerReport> TimerSection::GetReport(bool recursive) {
    auto* total     = TimerStatTotal::GetTimerStatsTotal();
    auto  read_lock = total->LockForRead();
    auto  report    = total->GetReport(name_, collector_index_);
    if (recursive) {
        for (auto&& entry : subsections_) {
            report->AddSubsection(entry.second->GetReport(recursive));
        }
    }
    return report;
}

TimerSection* TimerSection::SubSection(const std::string& name) {
    auto search = subsections_.find(name);
    if (search != subsections_.end()) {
        return search->second.get();
    }

    auto insert = subsections_.emplace(
        name,
        std::make_unique<TimerSection>(name, collector_index_count.fetch_add(1), CtorPermission()));
    return insert.first->second.get();
}

void TimerSection::ReportDurationNsec(cr_timestamp_nsec_t duration) {
    TimerStatCollector::GetTimerStatsCollector()->Report(collector_index_, duration);
}

}  // namespace cris::core

#endif  // ENABLE_PROFILING
