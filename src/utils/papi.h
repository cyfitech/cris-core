#pragma once

#if defined(CRIS_USE_PAPI) && CRIS_USE_PAPI
#if defined(__has_include) && __has_include(<papi.h>)
#include <papi.h>
#else
#undef CRIS_USE_PAPI
#endif
#endif

#include <atomic>
#include <sstream>
#include <string>
#include <vector>

namespace cris::core {

class PAPIStat {
   public:
    template<typename T>
    using atomic = std::atomic<T>;

    using string = std::string;

    using ostringstream = std::ostringstream;

    template<typename T>
    using vector = std::vector<T>;

    using Self = PAPIStat;

    PAPIStat();
    ~PAPIStat();

    void Start();
    void Stop();
    void Reset();

    explicit operator string();

    // These are inmuutable but C API does not allow const.
    vector<int> papi_events_ = vector<int> {
#if defined(CRIS_USE_PAPI) && CRIS_USE_PAPI
        PAPI_L1_TCM, PAPI_L2_TCM, PAPI_L3_TCM, PAPI_TLB_DM, PAPI_TLB_IM, PAPI_MEM_WCY, PAPI_RES_STL,
#endif
    };
    vector<long long> values_ = vector<long long>(papi_events_.size());

    bool auto_print_ = true;

    static atomic<bool> enabled_;

   protected:
    static unsigned long get_tid();

    int event_set_ =
#if defined(CRIS_USE_PAPI) && CRIS_USE_PAPI
        PAPI_NULL;
#else
        0;
#endif

    ostringstream buf_;
};

}  // namespace cris::core
