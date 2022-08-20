#ifdef ENABLE_PROFILING

#include "cris/core/timer/timer.h"
#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

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

// 0 is reserved as an "empty" collector.
constinit std::atomic<std::size_t> TimerSection::collector_index_count{1};

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

    static TimerStatCollector& GetCollector();

    static TimerStatCollector& GetTotalStats();

    static void FlushCollectedStats();

   private:
    struct CollectorList {
        using collectors_t = std::vector<std::unique_ptr<TimerStatCollector>>;

        template<class Func>
        auto LockAndThen(Func&& f) -> decltype(f(std::declval<collectors_t&>())) {
            std::lock_guard lock(mtx_);
            return f(collectors_);
        }

        std::mutex                                       mtx_;
        std::vector<std::unique_ptr<TimerStatCollector>> collectors_;
    };

    static CollectorList& GetCollectorList();

    using collector_entries_t = std::array<CollectorEntry, kCollectorSize>;

    mutable std::mutex                   mutex_;
    cr_timestamp_nsec_t                  last_clear_time_;
    cr_timestamp_nsec_t                  last_merge_time_;
    std::unique_ptr<collector_entries_t> collector_entries_;
};

TimerStatCollector::TimerStatCollector()
    : last_clear_time_(GetSystemTimestampNsec())
    , last_merge_time_(last_clear_time_)
    , collector_entries_(std::make_unique<collector_entries_t>()) {
}

TimerStatCollector::TimerStatCollector(const TimerStatCollector& another)
    : last_clear_time_(another.last_clear_time_)
    , last_merge_time_(another.last_merge_time_)
    , collector_entries_(std::make_unique<collector_entries_t>(*another.collector_entries_)) {
}

TimerStatCollector::CollectorList& TimerStatCollector::GetCollectorList() {
    static TimerStatCollector::CollectorList collector_list;
    return collector_list;
}

TimerStatCollector& TimerStatCollector::GetCollector() {
    static thread_local auto* const collector = []() {
        return GetCollectorList().LockAndThen(
            [](auto& collectors) { return collectors.emplace_back(new TimerStatCollector()).get(); });
    }();
    return *collector;
}

TimerStatCollector& TimerStatCollector::GetTotalStats() {
    static TimerStatCollector total;
    return total;
}

void TimerStatCollector::FlushCollectedStats() {
    GetCollectorList().LockAndThen([](auto& collectors) {
        for (auto& collector : collectors) {
            GetTotalStats().Merge(collector->Collect(/*clear = */ true));
        }
    });
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

    // If another thread is collecting data, then we simply skip this data point.
    //
    // Data collection is triggered by TimerSection::GetReport and TimerSection::GetAllReports
    // and either of them should not happen frequently.
    std::unique_lock lock(mutex_, std::defer_lock);
    if (!lock.try_lock()) {
        return;
    }

    auto& bucket = collector_entries_->at(index).duration_buckets_[bucket_idx];
    ++bucket.hits_;
    bucket.total_duration_ns_ += duration;
}

void TimerStatCollector::Merge(const TimerStatCollector& another) {
    std::scoped_lock lock(mutex_, another.mutex_);
    for (std::size_t i = 0; i < kCollectorSize; ++i) {
        collector_entries_->at(i).Merge(another.collector_entries_->at(i));
    }
    last_merge_time_ = GetSystemTimestampNsec();
}

void TimerStatCollector::UnsafeClear() {
    for (auto& entry : *collector_entries_) {
        for (auto& bucket : entry.duration_buckets_) {
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
    const auto& entry  = collector_entries_->at(section->collector_index_);

    report->section_name_    = section->name_;
    report->timing_duration_ = last_merge_time_ - last_clear_time_;
    for (std::size_t i = 0; i < kTimerEntryBucketNum; ++i) {
        const auto bucket_hits = entry.duration_buckets_[i].hits_;
        report->has_data_ |= static_cast<bool>(bucket_hits);
        report->report_buckets_.emplace_back(impl::TimerStatEntryBucket{
            .hits_              = bucket_hits,
            .total_duration_ns_ = static_cast<cr_duration_nsec_t>(entry.duration_buckets_[i].total_duration_ns_),
        });
    }

    if (recursive) {
        std::shared_lock lock(section->shared_mtx_);
        for (auto& subsection : section->subsections_) {
            auto subsection_report = GetReport(subsection.second.get(), recursive);
            report->has_data_ |= subsection_report->has_data_;
            report->AddSubsection(std::move(subsection_report));
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
    if (!has_data_) {
        return;
    }
    LOG(INFO) << indent << "Section '" << GetSectionName() << "':";
    if (GetTotalHits() > 0) {
        LOG(INFO) << indent << "    hits    : " << GetTotalHits() << " times, freq: " << GetFreq() << " Hz";
        LOG(INFO) << indent << "    avg time: " << std::fixed << std::setprecision(3)
                  << duration_cast<duration<double, std::micro>>(nanoseconds(GetAverageDurationNsec())).count()
                  << " us";
        auto print_percentile = [&](int percent) {
            LOG(INFO)
                << indent << "    " << percent << "p time: " << std::fixed << std::setprecision(3)
                << duration_cast<duration<double, std::micro>>(nanoseconds(GetPercentileDurationNsec(percent))).count()
                << " us";
        };
        print_percentile(50);
        print_percentile(90);
        print_percentile(95);
        print_percentile(99);
        LOG(INFO);
    }
    if (!subsections_.empty()) {
        LOG(INFO) << indent << "Subsections:";
        LOG(INFO);
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

    // Initializing stat total and all collectors by calling an empty flush.
    TimerStatCollector::FlushCollectedStats();

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
    TimerStatCollector::FlushCollectedStats();
    return TimerStatCollector::GetTotalStats().Collect(/* clear = */ false).GetReport(this, recursive);
}

std::unique_ptr<TimerReport> TimerSection::GetAllReports(bool clear) {
    TimerStatCollector::FlushCollectedStats();
    return TimerStatCollector::GetTotalStats().Collect(clear).GetReport(GetMainSection(), /* recursive = */ true);
}

TimerSection* TimerSection::SubSection(const std::string& name) {
    {
        std::shared_lock lock(shared_mtx_);
        auto             search = subsections_.find(name);
        if (search != subsections_.end()) {
            return search->second.get();
        }
    }

    std::unique_lock lock(shared_mtx_);
    auto             insert = subsections_.emplace(
        name,
        std::make_unique<TimerSection>(name, collector_index_count.fetch_add(1), CtorPermission()));
    return insert.first->second.get();
}

void TimerSection::ReportDurationNsec(cr_timestamp_nsec_t duration) {
    TimerStatCollector::GetCollector().Report(collector_index_, duration);
}

}  // namespace cris::core

#endif  // ENABLE_PROFILING
