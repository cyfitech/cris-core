#pragma once

#include "cris/core/utils/time.h"

#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wambiguous-reversed-operator"
#endif

#include <boost/lockfree/queue.hpp>

#if defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <cstddef>
#include <map>
#include <stack>
#include <string>

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-error)
#define CRIS_TIMELINE(sess)                                                  \
    static const ::std::string      cris_timeline_macro_session_name = sess; \
    ::cris::core::Timeline::Session cris_timeline_macro_session_raii(cris_timeline_macro_session_name.c_str())

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-error)
#define CRIS_TIMELINE_LOCAL(sess)                                                 \
    static const ::std::string           cris_timeline_macro_session_name = sess; \
    ::cris::core::Timeline::LocalSession cris_timeline_macro_session_raii(cris_timeline_macro_session_name.c_str())

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-error)
#define CRIS_FUNC_TIMELINE_LINE_STR(line) #line

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-error)
#define CRIS_FUNC_TIMELINE_UNIQ_NAME(suffix) cris_timeline_macro_region_name_##suffix

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-error)
#define CRIS_FUNC_TIMELINE_UNIQ_RAII(suffix) cris_timeline_macro_region_raii_##suffix

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-error)
#define CRIS_FUNC_TIMELINE_UNIQ(line)                                                        \
    static const ::std::string CRIS_FUNC_TIMELINE_UNIQ_NAME(line) =                          \
        ::std::string(__func__) + "() from " __FILE__ ":" CRIS_FUNC_TIMELINE_LINE_STR(line); \
    ::cris::core::Timeline::Region CRIS_FUNC_TIMELINE_UNIQ_RAII(line)(CRIS_FUNC_TIMELINE_UNIQ_NAME(line).c_str())

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-error)
#define CRIS_FUNC_TIMELINE() CRIS_FUNC_TIMELINE_UNIQ(__LINE__)

namespace cris::core {

class Timeline {
   public:
    template<typename K, typename V>
    using map = std::map<K, V>;

    using size_t = std::size_t;

    template<typename T>
    using stack = std::stack<T>;

    using string = std::string;

    struct Event {
        unsigned long long beg      = 0;
        unsigned long long end      = 0;
        long long          overhead = 0;
        const char*        sess     = nullptr;
        const char*        func     = nullptr;
    };

    class Session {
       public:
        using Self = Session;

        Session()            = delete;
        Session(const Self&) = delete;
        Session(Self&&)      = delete;
        Self& operator=(const Self&) = delete;
        Self& operator=(Self&&) = delete;

        explicit Session(const char* const sess) { start(sess); }

        ~Session() { pause(); }
    };

    class LocalSession {
       public:
        using Self = LocalSession;

        LocalSession()            = delete;
        LocalSession(const Self&) = delete;
        LocalSession(Self&&)      = delete;
        Self& operator=(const Self&) = delete;
        Self& operator=(Self&&) = delete;

        explicit LocalSession(const char* const sess) { start(sess); }

        ~LocalSession() {
            stop();
            print();
        }
    };

    class Region {
       public:
        using Self = Region;

        Region()            = delete;
        Region(const Self&) = delete;
        Region(Self&&)      = delete;
        Self& operator=(const Self&) = delete;
        Self& operator=(Self&&) = delete;

        explicit Region(const char* const func) : event_(push(func)) {}

        ~Region() { pop(event_); }

        Event* const event_ = nullptr;
    };

    Timeline()  = delete;
    ~Timeline() = default;

    static const char* start(const char* const sess);
    static const char* pause();
    static const char* stop();
    static Event*      push(const char* const func);
    static Event*      pop(Event* const event);
    static void        print();

   protected:
    template<typename T>
    static double tsc_to_us(const T tsc) {
        static const double us_per_tsc = kTscToNsecRatio * 1e-3;
        return static_cast<double>(tsc) * us_per_tsc;
    };

    static thread_local stack<const char*>       sess_stack_;
    static thread_local map<const char*, size_t> sess_cnt_;
    static boost::lockfree::queue<const char*>   sess_ready_;
    static boost::lockfree::queue<Event*>        events_;
    static thread_local map<string, size_t>      str_pool;
};

}  // namespace cris::core
