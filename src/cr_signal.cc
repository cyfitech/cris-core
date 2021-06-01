#include <glog/logging.h>
#include <signal.h>

#include <string>

#include "cris/core/timer/timer.h"

namespace cris::core {

static void WriteToGlog(const char *data, int size) {
    std::string msg(data, size);
    LOG(ERROR) << msg;
}

// Invoke the default signal handler.
static void InvokeDefaultSignalHandler(int signal_number) {
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(sig_action));
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_handler = SIG_DFL;
    sigaction(signal_number, &sig_action, NULL);
    kill(getpid(), signal_number);
}

static std::atomic<pthread_t *> current_thread_in_handler{nullptr};

static void SigIntHandler(int signal_number, siginfo_t *signal_info, void *ucontext) {
    TimerSection::FlushCollectedStats();
    TimerSection::GetMainSection()->GetReport()->PrintToLog();
}

static void SignalHandler(int signal_number, siginfo_t *signal_info, void *ucontext) {
    pthread_t  current_thread            = pthread_self();
    pthread_t *expect_current_in_handler = nullptr;
    if (!current_thread_in_handler.compare_exchange_strong(expect_current_in_handler,
                                                           &current_thread)) {
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

    switch (signal_number) {
        case SIGINT:
            SigIntHandler(signal_number, signal_info, ucontext);
            break;
        default:
            break;
    }

    InvokeDefaultSignalHandler(signal_number);
}

void InstallSigIntHandler() {
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(sig_action));
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags |= SA_SIGINFO;
    sig_action.sa_sigaction = &SignalHandler;

    CHECK_ERR(sigaction(SIGINT, &sig_action, NULL));
}

void InstallSignalHandler() {
    InstallSigIntHandler();
    google::InstallFailureSignalHandler();
    google::InstallFailureWriter(WriteToGlog);
}

}  // namespace cris::core
