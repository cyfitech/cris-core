#include "cris/core/timer/simple_timer.h"

#include <chrono>
#include <cstddef>
#include <functional>

namespace cris::core {

SimpleTimer::SimpleTimer() : epoch_{GetSystemTimestampNsec()} {
}

void SimpleTimer::Start() {
    epoch_ = GetSystemTimestampNsec();
}

std::int64_t SimpleTimer::TimeElapsedNs() const {
    return GetSystemTimestampNsec() - epoch_;
}

ScopedTimer::ScopedTimer(Closure&& closure) : closure_{std::move(closure)} {
}

ScopedTimer::~ScopedTimer() {
    closure_(*this);
}

}  // namespace cris::core
