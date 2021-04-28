#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>

namespace cris::core {

using cr_timestamp_nsec_t = int64_t;
using cr_dutration_nsec_t = int64_t;

cr_timestamp_nsec_t GetSystemTimestampNsec();

class TimerReport {
   public:
    TimerReport() = default;

    TimerReport(const std::string& name, uint64_t hits, cr_timestamp_nsec_t total_duration);

    void AddSubsection(std::unique_ptr<TimerReport>&& subsection);

    std::string GetSectionName() const;

    uint64_t GetHits() const;

    cr_dutration_nsec_t GetAverageDurationNsec() const;

   private:
    std::string                                         mSectionName;
    uint64_t                                            mHits;
    cr_dutration_nsec_t                                 mTotalDuration;
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

    TimerSession StartTimerSession();

    std::unique_ptr<TimerReport> GetReport(bool recursive = true);

    TimerSection* SubSection(const std::string& name);

    template<class duration_t>
    void ReportDuration(duration_t&& duration);

    void ReportDurationNsec(cr_dutration_nsec_t duration);

    static TimerSection* GetMainSection();

   private:
    std::string                                          mName;
    size_t                                               mCollectorIndex;
    std::map<std::string, std::unique_ptr<TimerSection>> mSubsections;

    static std::atomic<size_t> collector_index_count;
};

template<class duration_t>
void ReportDuration(duration_t&& duration) {
    ReportDurationNsec(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::forward<duration_t>(duration))
            .count());
}

}  // namespace cris::core
