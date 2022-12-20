#pragma once

#include "cris/core/utils/time.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

namespace cris::core {

// Clang has the unused check for private field,
// while GCC does not allow [[maybe_unused]] attribute on private fields
#ifdef __clang__
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-errors)
#define PRIVATE_MAYBE_UNUSED maybe_unused
#else
#define PRIVATE_MAYBE_UNUSED
#endif

namespace impl {

struct TimerStatEntryBucket {
    void Merge(const TimerStatEntryBucket& another) {
        hits_ += another.hits_;
        total_duration_ns_ += another.total_duration_ns_;
    }

    void Clear() {
        hits_              = 0;
        total_duration_ns_ = 0;
    }

    unsigned long long hits_{0};
    cr_duration_nsec_t total_duration_ns_{0};
};

}  // namespace impl

class TimerReport {
   public:
    TimerReport() = default;

    void AddSubsection(std::unique_ptr<TimerReport>&& subsection);

    std::string GetSectionName() const;

    unsigned long long GetTotalHits() const;

    double GetFreq() const;

    cr_duration_nsec_t GetAverageDurationNsec() const;

    cr_duration_nsec_t GetPercentileDurationNsec(int percent) const;

    void PrintToLog(unsigned indent_level = 0) const;

    bool                                                has_data_{false};
    std::string                                         section_name_;
    cr_duration_nsec_t                                  timing_duration_{0};
    std::vector<impl::TimerStatEntryBucket>             report_buckets_;
    cr_duration_nsec_t                                  max_duration_ns_{0};
    std::map<std::string, std::unique_ptr<TimerReport>> subsections_;
};

class TimerSession {
   public:
    TimerSession() = default;

    TimerSession(cr_timestamp_nsec_t started_timestamp, std::size_t collector_index);

    TimerSession(TimerSession&& session);

    TimerSession(const TimerSession&) = delete;
    TimerSession& operator=(const TimerSession&) = delete;
    TimerSession& operator=(TimerSession&& session) = delete;

    ~TimerSession();

    void EndSession();

   private:
    bool                is_ended_{false};
    cr_timestamp_nsec_t started_timestamp_{0};
    std::size_t         collector_index_{0};
};

class TimerSection {
   private:
    struct CtorPermission {};

   public:
    // The CtorPermission class has to be visible to caller, so that the class
    // cannot be constructed externally.
    TimerSection(const std::string& name, std::size_t collector_index, CtorPermission);

    TimerSection(const TimerSection&) = delete;
    TimerSection(TimerSection&&)      = delete;
    TimerSection& operator=(const TimerSection&) = delete;
    TimerSection& operator=(TimerSection&&) = delete;

    ~TimerSection() = default;

    [[nodiscard]] TimerSession StartTimerSession();

    std::unique_ptr<TimerReport> GetReport(bool recursive = true);

    // Try to get subsections in initializtion. This function tries to grab a lock,
    // which may lead to performance degradation.
    TimerSection* SubSection(const std::string& name);

    template<class duration_t>
    void ReportDuration(duration_t&& duration);

    void ReportDurationNsec(cr_duration_nsec_t duration);

    static TimerSection* GetMainSection();

    static std::unique_ptr<TimerReport> GetAllReports(bool clear);

   private:
    std::string                                          name_;
    [[PRIVATE_MAYBE_UNUSED]] std::size_t                 collector_index_{0};
    [[PRIVATE_MAYBE_UNUSED]] std::shared_mutex           shared_mtx_;
    std::map<std::string, std::unique_ptr<TimerSection>> subsections_;

    static std::atomic<std::size_t> collector_index_count;

    friend class TimerStatCollector;
};

#undef PRIVATE_MAYBE_UNUSED

template<class duration_t>
void TimerSection::ReportDuration(duration_t&& duration) {
    ReportDurationNsec(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::forward<duration_t>(duration)).count());
}

}  // namespace cris::core
