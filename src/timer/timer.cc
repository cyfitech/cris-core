#include "cris/core/timer/timer.h"

namespace cris::core {

cr_timestamp_nsec_t GetSystemTimestampNsec() {
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::steady_clock;
    return static_cast<cr_timestamp_nsec_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

TimerSection* TimerSection::GetMainSection() {
    static TimerSection main_section;
    return &main_section;
}

TimerSection *TimerSection::SubSection(const std::string &name) {
    // TODO
    return nullptr;
}

#ifdef ENABLE_PROFILING

// TODO

#else

TimerSection::TimerSection() {
}

TimerSection::~TimerSection() {
}

TimerSession TimerSection::StartTimerSession() {
    return {};
}

TimerReport TimerSection::GetReport(bool recursive) {
    return {};
}

void TimerSection::ReportDurationNsec(cr_timestamp_nsec_t duration) {
}

#endif

}  // namespace cris::core
