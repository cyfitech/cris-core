#include "cris/core/timer/timer.h"

#include <utility>

namespace cris::core {

TimerSession::TimerSession(TimerSession&& session)
    : is_ended_(std::exchange(session.is_ended_, true))
    , started_timestamp_(session.started_timestamp_)
    , collector_index_(session.collector_index_) {
}

}  // namespace cris::core
