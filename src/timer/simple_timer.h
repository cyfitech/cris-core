#pragma once

#include "cris/core/utils/time.h"

#include <chrono>
#include <cstddef>
#include <functional>

namespace cris::core {

class SimpleTimer {
   public:
    SimpleTimer();
    ~SimpleTimer() = default;

    void Start();

    cr_timestamp_nsec_t TimeElapsedNs() const;

    template<typename Duration>
    Duration DurationCast() const {
        const auto time_elapsed_ns = TimeElapsedNs();
        return std::chrono::duration_cast<Duration>(std::chrono::nanoseconds{time_elapsed_ns});
    }

   protected:
    cr_timestamp_nsec_t epoch_{};
};

class ScopedTimer : public SimpleTimer {
   public:
    using Closure = std::function<void(const SimpleTimer&)>;

    explicit ScopedTimer(Closure&& closure);

    ~ScopedTimer();

   protected:
    Closure closure_;
};

}  // namespace cris::core
