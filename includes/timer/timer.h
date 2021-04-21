#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>

namespace cris::core {

using cr_timestamp_nsec_t = uint64_t;
using cr_dutration_nsec_t = uint64_t;

cr_timestamp_nsec_t GetSystemTimestampNsec();

class TimerReport {};

class TimerSession {};

class TimerSection {
   public:
    TimerSection(const TimerSection&) = delete;
    TimerSection(TimerSection&&)      = default;
    TimerSection& operator=(const TimerSection&) = delete;

    ~TimerSection();

    TimerSession StartTimerSession();

    TimerReport GetReport(bool recursive = true);

    TimerSection* SubSection(const std::string& name);

    template<class duration_t>
    void ReportDuration(duration_t&& duration);

    void ReportDurationNsec(cr_dutration_nsec_t duration);

    static TimerSection* GetMainSection();

   private:
    TimerSection();

    std::string                                          mName;
    size_t                                               mCollectorIndex;
    std::map<std::string, std::unique_ptr<TimerSection>> mSubSections;
};

template<class duration_t>
void ReportDuration(duration_t&& duration) {
    ReportDurationNsec(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::forward<duration_t>(duration))
            .count());
}

}  // namespace cris::core
