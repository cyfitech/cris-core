#include "cris/core/utils/timeline.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"
#include "cris/core/utils/time.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <iomanip>
#include <memory>
#include <numeric>
#include <sstream>

using namespace std;

namespace cris::core {

Timeline::Event* Timeline::push(const char* const func) {
    static thread_local unsigned aux = 0u;
    if (sess_stack_.empty()) [[unlikely]] {
        return nullptr;
    }
    const auto tic   = GetTSCTick(aux);
    auto       event = make_unique<Event>(Event{
        .beg  = tic,
        .end  = tic,
        .sess = sess_stack_.top(),
        .func = func,
    });
    events_.push(event.get());
    const auto toc = GetTSCTick(aux);

    // Overhead from 3 RDTSC (including pop) and lock-free queue.
    event->overhead = static_cast<long long>((GetTSCTick(aux) - toc) * 3 + (toc - tic));
    event->end      = toc;
    return event.release();
}

Timeline::Event* Timeline::pop(Event* const event) {
    static thread_local unsigned aux = 0u;
    if (event) {
        event->end = GetTSCTick(aux);
    }
    return event;
}

const char* Timeline::start(const char* const sess) {
    auto& cnt = sess_cnt_[sess];
    if (!cnt) {
        sess_stack_.push(sess);
    } else if (sess_stack_.top() != sess) [[unlikely]] {
        LOG(ERROR) << "Interleaving timeline \"" << sess << "\" within thread is not supported.";
        return nullptr;
    }
    ++cnt;
    return sess;
}

const char* Timeline::pause() {
    const char* const sess = sess_stack_.top();
    auto&             cnt  = sess_cnt_[sess];
    if (!cnt) [[unlikely]] {
        LOG(ERROR) << "Timeline \"" << sess << "\" is not registered to thread.";
        return nullptr;
    }
    if (!--cnt) {
        sess_stack_.pop();
    }
    return sess;
}

const char* Timeline::stop() {
    const char* const sess = sess_stack_.top();
    auto&             cnt  = sess_cnt_[sess];
    if (!cnt) [[unlikely]] {
        LOG(ERROR) << "Timeline \"" << sess << "\" is not registered to thread.";
        return nullptr;
    }
    if (!--cnt) {
        sess_ready_.push(sess);
        sess_stack_.pop();
    }
    return sess;
}

void Timeline::print() {
    // Snapshot.
    deque<const char*> sess_sel;
    deque<Event*>      evt_sel;
    sess_ready_.consume_all([&sess_sel](const char* const sess) { sess_sel.push_back(sess); });
    events_.consume_all([&evt_sel](Event* const evt) { evt_sel.push_back(evt); });

    // Group by session.
    unordered_map<const char*, vector<Event*>> sess_evt;
    for (const char* const sess : sess_sel) {
        sess_evt[sess];
    }
    for (Event* const evt : evt_sel) {
        if (evt->beg > evt->end) [[unlikely]] {
            LOG(ERROR) << "Drop invalid event \"" << evt->func << "\" [" << evt->beg << ", " << evt->end
                       << "] in session \"" << evt->sess << "\".";
            continue;
        }
        auto iter = sess_evt.find(evt->sess);
        if (iter != sess_evt.end()) {
            iter->second.push_back(evt);
        } else {
            events_.push(evt);
        }
    }
    ostringstream buf;
    for (auto& entry : sess_evt) {
        auto& chain = entry.second;
        if (chain.empty()) {
            continue;
        }
        sort(chain.begin(), chain.end(), [](Event* const lhs, Event* const rhs) { return lhs->beg < rhs->beg; });

        stack<pair<unique_ptr<Event>, long long>> dfs;
        const auto                                off = chain.front()->beg;
        const auto                                dur = chain.back()->end - off;

        // 4-char alignment for 3 decimals.
        const int width = static_cast<int>(to_string(static_cast<long long>(ceil(tsc_to_us(dur)))).size() + 7) >> 2
                << 2;

        buf.str();
        buf.clear();
        buf << setprecision(3) << fixed;
        {
            const long long overhead = accumulate(
                entry.second.begin(),
                entry.second.end(),
                0ll,
                [](const long long acc, const Event* const evt) { return acc + evt->overhead; });
            buf << "Timeline session \"" << entry.first << "\" took "
                << tsc_to_us(static_cast<long long>(dur) - overhead) << '+' << tsc_to_us(overhead) << " us:";
        }

        const string vsplit = '+' + string((static_cast<size_t>(width) << 1) + 3, '-') + '+' +
            string(static_cast<size_t>(width) + 2, '-') + '+' + string(30, '-');
        buf << "\n    " << vsplit;
        buf << "\n    |" << setw(width + 1) << "dur" << setw(width + 2) << left << "+err" << right << '|'
            << setw(width + 2) << "clk (us) "
            << "| stack frame";
        buf << "\n    " << vsplit;

        auto draw = [&buf, &dfs, off, width](const bool pop) {
            const auto& evt      = *dfs.top().first;
            const auto  overhead = dfs.top().second;
            buf << "\n    |";
            if (pop) {
                buf << setw(width + 1) << tsc_to_us(static_cast<long long>(evt.end - evt.beg) - overhead) << '+'
                    << setw(width + 1) << left << tsc_to_us(overhead) << right;
            } else {
                buf << setw((width << 1) + 3) << ' ';
            }
            buf << '|' << setw(width + 1) << tsc_to_us((pop ? evt.end : evt.beg) - off) << " |"
                << string(dfs.size() - 1, '|') << (pop ? '/' : '\\') << ' ' << evt.func;
        };
        for (Event* const evt : entry.second) {
            for (long long overhead = 0; !dfs.empty(); dfs.pop()) {
                overhead = (dfs.top().second += overhead);

                // Assume instant boundary event and partially overlapped event to be child of the early one.
                if (min(evt->beg + 1, evt->end) <= dfs.top().first->end) {
                    break;
                }
                draw(true);
            }
            dfs.emplace(evt, evt->overhead);
            draw(false);
        }
        for (long long overhead = 0; !dfs.empty(); dfs.pop()) {
            overhead = (dfs.top().second += overhead);
            draw(true);
        }
        buf << "\n    " << vsplit;

        LOG(INFO) << buf.str();
        buf.str();
        buf.clear();
    }
}

thread_local stack<const char*>          Timeline::sess_stack_;
thread_local map<const char*, size_t>    Timeline::sess_cnt_;
boost::lockfree::queue<const char*>      Timeline::sess_ready_(1024);
boost::lockfree::queue<Timeline::Event*> Timeline::events_(1024);
thread_local map<string, size_t>         str_pool;

}  // namespace cris::core
