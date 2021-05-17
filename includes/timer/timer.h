#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>

namespace cris::core {

using cr_timestamp_nsec_t = int64_t;
using cr_dutration_nsec_t = int64_t;

// monotonic timestamp
cr_timestamp_nsec_t GetSystemTimestampNsec();

// real-world timestamp
cr_timestamp_nsec_t GetUnixTimestampNsec();

class TimerReport {
   public:
    TimerReport() = default;

    void AddSubsection(std::unique_ptr<TimerReport>&& subsection);

    std::string GetSectionName() const;

    uint64_t GetHits() const;

    double GetFreq() const;

    cr_dutration_nsec_t GetAverageDurationNsec() const;

    void PrintToLog(int indent_level = 0) const;

    std::string                                         mSectionName;
    uint64_t                                            mHits;
    cr_dutration_nsec_t                                 mSessionDurationSum;
    cr_dutration_nsec_t                                 mTimingDuration;
    std::map<std::string, std::unique_ptr<TimerReport>> mSubsections;
};

class TimerSession {
   public:
    TimerSession() = default;

    TimerSession(cr_timestamp_nsec_t started_timestamp, size_t collector_index);

    ~TimerSession();

    void EndSession();

   private:
    [[maybe_unused]] bool                mIsEnded{false};
    [[maybe_unused]] cr_timestamp_nsec_t mStartedTimestamp;
    [[maybe_unused]] size_t              mCollectorIndex;
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
    std::string                                          mName;
    [[maybe_unused]] size_t                              mCollectorIndex;
    std::map<std::string, std::unique_ptr<TimerSection>> mSubsections;

    static std::atomic<size_t> collector_index_count;
};

template<class duration_t>
void TimerSection::ReportDuration(duration_t&& duration) {
    ReportDurationNsec(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::forward<duration_t>(duration))
            .count());
}

}  // namespace cris::core
