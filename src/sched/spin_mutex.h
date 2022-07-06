#pragma once

#include <atomic>

namespace cris::core {

// Unlike `std::unique_lock`, its `try_lock` does not have false-negative, but its `lock` will keep spinning for a
// while. So it is not recommanded unless you need a strong `try_lock` and will not hold the lock for too long.
class HybridSpinMutex {
   public:
    using Self = HybridSpinMutex;

    HybridSpinMutex()            = default;
    HybridSpinMutex(const Self&) = delete;
    HybridSpinMutex(Self&&)      = delete;
    HybridSpinMutex& operator=(const Self&) = delete;
    HybridSpinMutex& operator=(Self&&) = delete;
    ~HybridSpinMutex()                 = default;

    void lock();

    void unlock();

    bool try_lock();

   private:
    std::atomic_flag locked_ = ATOMIC_FLAG_INIT;
};

}  // namespace cris::core
