#include "cris/core/utils/papi.h"

#include "cris/core/utils/logging.h"

#include <sstream>
#include <stdexcept>
#include <thread>

using namespace std;

namespace cris::core {

PAPIStat::PAPIStat() {
    if (!enabled_) {
        return;
    }
#if defined(CRIS_USE_PAPI) && CRIS_USE_PAPI
    {
        const int err = PAPI_create_eventset(&event_set_);
        if (err != PAPI_OK) {
            LOG(WARNING) << "PAPI failed (" << err << ") to create event set.";
            enabled_ = false;
            return;
        }
    }
    {
        const int err = PAPI_assign_eventset_component(event_set_, 0);
        if (err != PAPI_OK) {
            LOG(WARNING) << "PAPI failed (" << err << ") to bind event set to CPU.";
            enabled_ = false;
            return;
        }
    }
    {
        const int err = PAPI_set_multiplex(event_set_);
        if (err != PAPI_OK) {
            LOG(WARNING) << "PAPI failed (" << err << ") to create multiplex event set.";
            enabled_ = false;
            return;
        }
    }
    {
        const int err = PAPI_add_events(event_set_, papi_events_.data(), static_cast<int>(papi_events_.size()));
        if (err != PAPI_OK) {
            LOG(WARNING) << "PAPI failed (" << err << ") to add event(s).";
            enabled_ = false;
            return;
        }
    }
#else
    LOG(WARNING) << "PAPI is disabled in build.";
#endif
    Start();
}

PAPIStat::~PAPIStat() {
    Stop();
#if defined(CRIS_USE_PAPI) && CRIS_USE_PAPI
    if (event_set_ != PAPI_NULL) {
        const int err = PAPI_destroy_eventset(&event_set_);
        if (err != PAPI_OK) {
            LOG(WARNING) << "PAPI failed (" << err << ") to free the event set.";
            enabled_ = false;
            return;
        }
    }
#endif
    if (auto_print_) {
        LOG(INFO) << operator string();
    }
}

void PAPIStat::Start() {
    if (!enabled_) {
        return;
    }
#if defined(CRIS_USE_PAPI) && CRIS_USE_PAPI
    CHECK_EQ(PAPI_start(event_set_), PAPI_OK) << "PAPI failed to start." << endl;
#endif
}

void PAPIStat::Stop() {
#if defined(CRIS_USE_PAPI) && CRIS_USE_PAPI
    if (enabled_) {
        const int err = PAPI_stop(event_set_, values_.data());
        if (err != PAPI_OK) {
            LOG(WARNING) << "PAPI failed (" << err << ") to stop.";
            enabled_ = false;
            return;
        }
    }
    if (event_set_ != PAPI_NULL) {
        const int err = PAPI_cleanup_eventset(event_set_);
        if (err != PAPI_OK) {
            LOG(WARNING) << "PAPI failed (" << err << ") to clean up the event set.";
            enabled_ = false;
            return;
        }
    }
#endif
}

void PAPIStat::Reset() {
    Stop();
    Start();
}

PAPIStat::operator string() {
    if (!enabled_) {
        return "PAPI disabled.";
    }
    buf_.clear();
    buf_.str("");
    buf_ << "PAPI summary:" << endl;
    buf_ << "    L1/2/3 cache misses:  " << values_[0] << " " << values_[1] << " " << values_[2] << endl;
    buf_ << "    Data/Inst TLB misses: " << values_[3] << " " << values_[4] << endl;
    buf_ << "    Stall on write/total: " << values_[5] << " " << values_[6];
    auto summary = buf_.str();
    buf_.clear();
    buf_.str("");
    return summary;
}

unsigned long PAPIStat::get_tid() {
    ostringstream buf;
    buf << this_thread::get_id();
    try {
        return static_cast<unsigned long>(stoul(buf.str()));
    } catch (const invalid_argument& ex) {
        LOG(WARNING) << "Cannot retrieve numerical thread id from \"" << buf.str()
                     << "\" to enable PAPI threading support.";
    } catch (const out_of_range& ex) {
        LOG(WARNING) << "Cannot retrieve numerical thread id from \"" << buf.str()
                     << "\" to enable PAPI threading support.";
    }
    return 0;
}

atomic<bool> PAPIStat::enabled_ = []() {
#if defined(CRIS_USE_PAPI) && CRIS_USE_PAPI
    {
        const int ver = PAPI_library_init(PAPI_VER_CURRENT);
        if (ver != static_cast<int>(PAPI_VER_CURRENT)) {
            LOG(WARNING) << "PAPI has already been initialized by another version instead of \"" << PAPI_VER_CURRENT
                         << "\".";
            return false;
        }
    }
    {
        const int err = PAPI_multiplex_init();
        if (err != PAPI_OK) {
            LOG(WARNING) << "PAPI disabled after failure (" << err << ") of init counter multiplexing.";
            return false;
        }
    }
    {
        const int err = PAPI_thread_init(get_tid);
        if (err != PAPI_OK) {
            LOG(WARNING) << "PAPI failed (" << err << ") to initialize threading support.";
            return false;
        }
    }
    {
        // A misaligned 7ms within typical time slice.
        static PAPI_option_t mpx_opt{
            .multiplex = PAPI_multiplex_option_t{
                .eventset = 0,
                .ns       = 7654321,
                .flags    = 0,
            }};
        // TODO(xkszltl): Somehow the option cannot be set.
        // NOLINTNEXTLINE(readability-simplify-boolean-expr,-warnings-as-errors)
        if constexpr (false) {
            const int err = PAPI_set_opt(PAPI_DEF_MPX_NS, &mpx_opt);
            if (err != PAPI_OK) {
                LOG(WARNING) << "PAPI failed to set multiplexing options.";
                return false;
            }
        }
    }
    return true;
#else
    return false;
#endif
}();

}  // namespace cris::core
