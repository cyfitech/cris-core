#include "cris/core/cr_signal.h"

#include "cris/core/logging.h"
#include "cris/core/timer.h"

#include <boost/stacktrace.hpp>

#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <string>

namespace cris::core {

static void WriteToGlog(const char* data, std::size_t size) {
    std::string msg(data, size);
    LOG(ERROR) << msg;
}

// Other signals may be captured by the default glog handler
static const int kFailureSignals[] = {
    SIGINT,
    SIGTRAP,
};

// Invoke the default signal handler.
static void InvokeDefaultSignalHandler(int signal_number) {
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(sig_action));
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_handler = SIG_DFL;
    sigaction(signal_number, &sig_action, NULL);
    kill(getpid(), signal_number);
}

static void DumpSignalInfo(int signal_number, siginfo_t* siginfo) {
    constexpr std::size_t kBufferLen = 1024;
    char                  buffer[kBufferLen];

    LOG(ERROR) << "******************************************************";

    snprintf(
        buffer,
        kBufferLen,
        "**** Signal %d (%s) received (@0x%" PRIxPTR ") ****",
        signal_number,
        strsignal(signal_number),
        reinterpret_cast<intptr_t>(siginfo->si_addr));

    LOG(ERROR) << buffer;

    snprintf(
        buffer,
        kBufferLen,
        "PID %d, TID 0x%" PRIxPTR ", from PID %d",
        getpid(),
        static_cast<intptr_t>(pthread_self()),
        siginfo->si_pid);

    LOG(ERROR) << buffer;
}

static void DumpStacktrace() {
    LOG(ERROR);
    LOG(ERROR) << "******************************************************";
    LOG(ERROR) << "*******************   STACKTRACE   *******************";
    LOG(ERROR) << "******************************************************";
    LOG(ERROR) << "\n" << boost::stacktrace::stacktrace();
}

static std::atomic<pthread_t*> current_thread_in_handler{nullptr};

static void SigIntHandler(int /* signal_number */, siginfo_t* /* signal_info */, void* /* ucontext */) {
    TimerSection::FlushCollectedStats();
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

    switch (signal_number) {
        case SIGINT:
            SigIntHandler(signal_number, signal_info, ucontext);
            break;
        default:
            DumpStacktrace();
            break;
    }

    InvokeDefaultSignalHandler(signal_number);
}

static void InstallFailureSignalHandler() {
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(sig_action));
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags |= SA_SIGINFO;
    sig_action.sa_sigaction = &SignalHandler;

    for (std::size_t i = 0; i < sizeof(kFailureSignals) / sizeof(kFailureSignals[0]); ++i) {
        CHECK_ERR(sigaction(kFailureSignals[i], &sig_action, NULL));
    }
}

void InstallSignalHandler() {
    InstallFailureSignalHandler();
    google::InstallFailureSignalHandler();
    google::InstallFailureWriter(WriteToGlog);
}

}  // namespace cris::core
