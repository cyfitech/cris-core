#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cris::core {

using cr_timestamp_nsec_t = int64_t;
using cr_dutration_nsec_t = int64_t;

// monotonic timestamp
cr_timestamp_nsec_t GetSystemTimestampNsec();

// real-world timestamp
cr_timestamp_nsec_t GetUnixTimestampNsec();

// Clang has the unused check for private field,
// while GCC does not allow [[maybe_unused]] attribute on private fields
#ifdef __clang__
#define PRIVATE_MAYBE_UNUSED [[maybe_unused]]
#else
#define PRIVATE_MAYBE_UNUSED
#endif

class TimerReport {
   public:
    TimerReport() = default;

    void AddSubsection(std::unique_ptr<TimerReport>&& subsection);

    std::string GetSectionName() const;

    uint64_t GetTotalHits() const;

    double GetFreq() const;

    cr_dutration_nsec_t GetAverageDurationNsec() const;

    cr_dutration_nsec_t GetPercentileDurationNsec(int percent) const;

    void PrintToLog(int indent_level = 0) const;

    struct TimerReportBucket {
        uint64_t            hits_;
        cr_dutration_nsec_t session_duration_sum_;
    };

    std::string                                         section_name_;
    cr_dutration_nsec_t                                 timing_duration_;
    std::vector<TimerReportBucket>                      report_buckets_;
    std::map<std::string, std::unique_ptr<TimerReport>> subsections_;
};

class TimerSession {
   public:
    TimerSession() = default;

    TimerSession(cr_timestamp_nsec_t started_timestamp, size_t collector_index);

    ~TimerSession();

    void EndSession();

   private:
    PRIVATE_MAYBE_UNUSED bool                is_ended_{false};
    PRIVATE_MAYBE_UNUSED cr_timestamp_nsec_t started_timestamp_;
    PRIVATE_MAYBE_UNUSED size_t              collector_index_;
};

class TimerSection {
   private:
    struct CtorPermission {};

   public:
    // The CtorPermission class has to be visible to caller, so that the class
    // cannot be constructed externally.
    TimerSection(const std::string& name, size_t collector_index, CtorPermission);

    TimerSection(const TimerSection&) = delete;
    TimerSection(TimerSection&&)      = default;
    TimerSection& operator=(const TimerSection&) = delete;

    [[nodiscard]] TimerSession StartTimerSession();

    std::unique_ptr<TimerReport> GetReport(bool recursive = true);

    TimerSection* SubSection(const std::string& name);

    template<class duration_t>
    void ReportDuration(duration_t&& duration);

    void ReportDurationNsec(cr_dutration_nsec_t duration);

    static TimerSection* GetMainSection();

    // When profiling, some of the stats may be cached in a separate data structure
    // to increase performance. This call flush those data so that they appear on the
    // report.
    static void FlushCollectedStats();

   private:
    std::string                                          name_;
    PRIVATE_MAYBE_UNUSED size_t                          collector_index_;
    std::map<std::string, std::unique_ptr<TimerSection>> subsections_;

    static std::atomic<size_t> collector_index_count;
};

#undef PRIVATE_MAYBE_UNUSED

template<class duration_t>
void TimerSection::ReportDuration(duration_t&& duration) {
    ReportDurationNsec(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::forward<duration_t>(duration)).count());
}

}  // namespace cris::core
