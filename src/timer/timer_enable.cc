#ifdef ENABLE_PROFILING

#include "cris/core/defs.h"
#include "cris/core/logging.h"
#include "cris/core/timer.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <mutex>
#include <shared_mutex>
#include <thread>

namespace cris::core {

constinit std::atomic<std::size_t> TimerSection::collector_index_count{0};

// Number of duration buckets in each Collector/Total Entry
static constexpr std::size_t kTimerEntryBucketNum = 32;

// Calculate the upper (exclusive) limit for each bucket.
// The bucket range grows exponentially. The range of the
// first bucket is 0 to base_nsec.
static constexpr cr_duration_nsec_t UpperNsecOfBucket(
    std::size_t        idx,
    cr_duration_nsec_t base_nsec = 10 * std::ratio_divide<std::micro, std::nano>::num /* 10 us */) {
    // The upper limit of the last bucket should be INT_MAX to cover everything
    if (idx >= kTimerEntryBucketNum - 1) {
        return std::numeric_limits<cr_duration_nsec_t>::max();
    }
    auto result = base_nsec;
    for (std::size_t i = 0; i < idx; ++i) {
        result *= 2;
    }
    return result;
}

template<std::size_t... idx>
static constexpr auto GenerateBucketUpperNsec(std::integer_sequence<std::size_t, idx...>)
    -> std::array<cr_duration_nsec_t, sizeof...(idx)> {
    return {UpperNsecOfBucket(idx)...};
}

// Upper limit nsec (exclusive) of duration buckets of each timer entry
static constexpr auto kBucketUpperNsec =
    GenerateBucketUpperNsec(std::make_integer_sequence<std::size_t, kTimerEntryBucketNum>{});

static_assert(kBucketUpperNsec[4] == 160000);
static_assert(kBucketUpperNsec[6] == 640000);
static_assert(kBucketUpperNsec[kTimerEntryBucketNum - 1] > 10000 * std::nano::den);

class TimerStatCollector {
   public:
    static constexpr std::size_t kCollectorNum  = 64;
    static constexpr std::size_t kCollectorSize = 8192;

    TimerStatCollector();

    TimerStatCollector(const TimerStatCollector& another);

    struct CollectorEntry {
        void Merge(const CollectorEntry& another);

        std::array<impl::TimerStatEntryBucket, kTimerEntryBucketNum> duration_buckets_{};
    };

    void Report(std::size_t index, cr_duration_nsec_t duration);

    void Merge(const TimerStatCollector& collector);

    void UnsafeClear();

    TimerStatCollector Collect(bool clear);

    std::unique_ptr<TimerReport> GetReport(TimerSection* section, bool recursive) const;

    static std::array<TimerStatCollector, kCollectorNum>& GetCollectors();

    static TimerStatCollector& GetCollector();

    static TimerStatCollector& GetTotalStats();

   private:
    mutable std::mutex                           mutex_;
    cr_timestamp_nsec_t                          last_clear_time_;
    cr_timestamp_nsec_t                          last_merge_time_;
    std::vector<std::unique_ptr<CollectorEntry>> collector_entries_;
};

TimerStatCollector::TimerStatCollector()
    : last_clear_time_(GetSystemTimestampNsec())
    , last_merge_time_(last_clear_time_) {
    for (std::size_t i = 0; i < kCollectorSize; ++i) {
        collector_entries_.push_back(std::make_unique<CollectorEntry>());
    }
}

TimerStatCollector::TimerStatCollector(const TimerStatCollector& another)
    : last_clear_time_(another.last_clear_time_)
    , last_merge_time_(another.last_merge_time_) {
    for (auto& entry : another.collector_entries_) {
        collector_entries_.push_back(std::make_unique<CollectorEntry>(*entry));
    }
}

std::array<TimerStatCollector, TimerStatCollector::kCollectorNum>& TimerStatCollector::GetCollectors() {
    static std::array<TimerStatCollector, kCollectorNum> collectors;
    return collectors;
}

TimerStatCollector& TimerStatCollector::GetCollector() {
    static const thread_local auto collector_idx =
        std::hash<std::thread::id>()(std::this_thread::get_id()) % GetCollectors().size();

    return GetCollectors()[collector_idx];
}

TimerStatCollector& TimerStatCollector::GetTotalStats() {
    static TimerStatCollector total;
    for (auto& collector : GetCollectors()) {
        total.Merge(collector.Collect(/*clear = */ true));
    }
    return total;
}

void TimerStatCollector::CollectorEntry::Merge(const TimerStatCollector::CollectorEntry& another) {
    for (std::size_t i = 0; i < duration_buckets_.size(); ++i) {
        duration_buckets_[i].Merge(another.duration_buckets_[i]);
    }
}

void TimerStatCollector::Report(std::size_t index, cr_duration_nsec_t duration) {
    if (duration < 0) {
        LOG(ERROR) << "duration " << duration << " is smaller than zero, skip it";
        return;
    }

    if (index >= kCollectorSize) {
        LOG(ERROR) << "index " << index << " is out of range";
        return;
    }

    // Linear scan is better than binary search here, since shorter
    // session should react faster, while it is ok for long sessions
    // to be a little slower
    std::size_t bucket_idx = 0;
    for (std::size_t i = 0; i < kTimerEntryBucketNum; ++i) {
        bucket_idx = i;
        if (duration < kBucketUpperNsec[i]) {
            break;
        }
    }

    // If another thread is collecting data, or it happens to be a collision in reporting,
    // we simply skip this data point.
    std::unique_lock lock(mutex_, std::defer_lock);
    if (!lock.try_lock()) {
        return;
    }

    auto& bucket = collector_entries_[index]->duration_buckets_[bucket_idx];
    ++bucket.hits_;
    bucket.total_duration_ns_ += duration;
}

void TimerStatCollector::Merge(const TimerStatCollector& another) {
    std::scoped_lock lock(mutex_, another.mutex_);
    for (std::size_t i = 0; i < kCollectorSize; ++i) {
        collector_entries_[i]->Merge(*another.collector_entries_[i]);
    }
    last_merge_time_ = GetSystemTimestampNsec();
}

void TimerStatCollector::UnsafeClear() {
    for (auto& entry : collector_entries_) {
        for (auto& bucket : entry->duration_buckets_) {
            bucket.Clear();
        }
    }
    last_clear_time_ = GetSystemTimestampNsec();
    last_merge_time_ = last_clear_time_;
}

TimerStatCollector TimerStatCollector::Collect(bool clear) {
    std::lock_guard lock(mutex_);

    TimerStatCollector collected_data(*this);

    if (clear) {
        UnsafeClear();
    }
    return collected_data;
}

std::unique_ptr<TimerReport> TimerStatCollector::GetReport(TimerSection* section, bool recursive) const {
    auto        report = std::make_unique<TimerReport>();
    const auto& entry  = *collector_entries_[section->collector_index_];

    report->section_name_    = section->name_;
    report->timing_duration_ = last_merge_time_ - last_clear_time_;
    for (std::size_t i = 0; i < kTimerEntryBucketNum; ++i) {
        report->report_buckets_.emplace_back(impl::TimerStatEntryBucket{
            .hits_              = entry.duration_buckets_[i].hits_,
            .total_duration_ns_ = static_cast<cr_duration_nsec_t>(entry.duration_buckets_[i].total_duration_ns_),
        });
    }

    if (recursive) {
        for (auto& subsection : section->subsections_) {
            report->AddSubsection(GetReport(subsection.second.get(), recursive));
        }
    }
    return report;
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

unsigned long long TimerReport::GetTotalHits() const {
    unsigned long long total_hits = 0;
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
    unsigned long long total_hits        = 0;
    cr_duration_nsec_t total_duration_ns = 0;
    for (const auto& bucket : report_buckets_) {
        total_hits += bucket.hits_;
        total_duration_ns += bucket.total_duration_ns_;
    }
    return static_cast<cr_duration_nsec_t>(static_cast<unsigned long long>(total_duration_ns) / total_hits);
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

    const auto total_hits  = GetTotalHits();
    const auto target_hits = static_cast<unsigned long long>(
        std::round(static_cast<double>(total_hits) * static_cast<double>(percent) / 100.));

    if (target_hits == 0) {
        return 0;
    }

    unsigned long long current_hits = 0;
    for (const auto& bucket : report_buckets_) {
        current_hits += bucket.hits_;
        if (current_hits >= target_hits) {
            return static_cast<cr_duration_nsec_t>(
                static_cast<unsigned long long>(bucket.total_duration_ns_) / bucket.hits_);
        }
    }

    LOG(ERROR) << __func__ << ": NEVER REACH HERE!!!!";
    return 0;
}

void TimerReport::PrintToLog(unsigned indent_level) const {
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

TimerSession::TimerSession(cr_timestamp_nsec_t started_timestamp, std::size_t collector_index)
    : started_timestamp_(started_timestamp)
    , collector_index_(collector_index) {
}

TimerSession::~TimerSession() {
    EndSession();
}

TimerSection* TimerSection::GetMainSection() {
    static TimerSection main_section("main", collector_index_count.fetch_add(1), {});

    // Initializing stat total to record the start time of profiling.
    [[maybe_unused]] static auto& total = TimerStatCollector::GetTotalStats();

    return &main_section;
}

TimerSection::TimerSection(const std::string& name, std::size_t collector_index, CtorPermission)
    : name_(name)
    , collector_index_(collector_index) {
}

void TimerSession::EndSession() {
    if (is_ended_) {
        return;
    }
    is_ended_     = true;
    auto duration = GetSystemTimestampNsec() - started_timestamp_;
    TimerStatCollector::GetCollector().Report(collector_index_, duration);
}

TimerSession TimerSection::StartTimerSession() {
    return TimerSession(GetSystemTimestampNsec(), collector_index_);
}

std::unique_ptr<TimerReport> TimerSection::GetReport(bool recursive) {
    return TimerStatCollector::GetTotalStats().Collect(/* clear = */ false).GetReport(this, recursive);
}

std::unique_ptr<TimerReport> TimerSection::GetAllReports(bool clear) {
    return TimerStatCollector::GetTotalStats().Collect(clear).GetReport(GetMainSection(), /* recursive = */ true);
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
    TimerStatCollector::GetCollector().Report(collector_index_, duration);
}

}  // namespace cris::core

#endif  // ENABLE_PROFILING
