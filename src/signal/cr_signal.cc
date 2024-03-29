#include "cris/core/signal/cr_signal.h"

#include "cris/core/timer/timer.h"
#include "cris/core/utils/logging.h"

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#include <glog/raw_logging.h>

#include <boost/stacktrace.hpp>

#include <unistd.h>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <atomic>
#include <cinttypes>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>

namespace cris::core {

static void WriteToGlog(const char* data, std::size_t size) {
    std::string msg(data, size);
    LOG(ERROR) << msg;
}

// Other signals may be captured by the default glog handler
static const int kFailureSignals[] = {
    SIGABRT,
    SIGILL,
    SIGINT,
    SIGSEGV,
    SIGTERM,
    SIGTRAP,
};

// Invoke the default signal handler.
static void InvokeDefaultSignalHandler(int signal_number) {
    struct sigaction sig_action {};
    memset(&sig_action, 0, sizeof(sig_action));
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_handler = SIG_DFL;
    sigaction(signal_number, &sig_action, NULL);
    kill(getpid(), signal_number);
}

static void DumpSignalInfo(int signal_number, siginfo_t* siginfo) {
    RAW_LOG(ERROR, "******************************************************");

    RAW_LOG(ERROR, "**** Signal %d received (@%p) ****", signal_number, siginfo->si_addr);

    std::stringstream tid;
    tid << std::this_thread::get_id();
    RAW_LOG(ERROR, "PID %d, TID %s, from PID %d", getpid(), tid.str().c_str(), siginfo->si_pid);
}

static void DumpStacktrace() {
    RAW_LOG(ERROR, "******************************************************");
    RAW_LOG(ERROR, "*******************   STACKTRACE   *******************");
    RAW_LOG(ERROR, "******************************************************");
    std::stringstream trace;
    trace << '\n';
    trace << boost::stacktrace::stacktrace();
    for (std::string trace_line; std::getline(trace, trace_line);) {
        RAW_LOG(ERROR, "%s", trace_line.c_str());
    }
}

static std::atomic<pthread_t*> current_thread_in_handler{nullptr};

static void SigIntHandler(int /* signal_number */, siginfo_t* /* signal_info */, void* /* ucontext */) {
    TimerSection::GetMainSection()->GetReport()->PrintToLog();
}

static void SignalHandler(int signal_number, siginfo_t* signal_info, void* ucontext) {
    pthread_t  current_thread            = pthread_self();
    pthread_t* expect_current_in_handler = nullptr;
    if (!current_thread_in_handler.compare_exchange_strong(expect_current_in_handler, &current_thread)) {
        if (pthread_equal(current_thread, *expect_current_in_handler)) {
            // Same thread was in the handler
            InvokeDefaultSignalHandler(signal_number);
        } else {
            // Another thread is dumping. Waiting until the process is killed
            while (true) {
                sleep(1);
            }
        }
    }

    DumpSignalInfo(signal_number, signal_info);

    if (signal_number == SIGINT) {
        SigIntHandler(signal_number, signal_info, ucontext);
    } else {
        DumpStacktrace();
    }

    InvokeDefaultSignalHandler(signal_number);
}

static void InstallFailureSignalHandler() {
    struct sigaction sig_action {};
    memset(&sig_action, 0, sizeof(sig_action));
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags |= SA_SIGINFO;
    sig_action.sa_sigaction = &SignalHandler;

    for (std::size_t i = 0; i < sizeof(kFailureSignals) / sizeof(kFailureSignals[0]); ++i) {
        CHECK_ERR(sigaction(kFailureSignals[i], &sig_action, NULL));
    }
}

void InstallSignalHandler() {
    google::InstallFailureSignalHandler();
    google::InstallFailureWriter(WriteToGlog);
    InstallFailureSignalHandler();
}

}  // namespace cris::core
