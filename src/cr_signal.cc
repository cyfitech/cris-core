#include <glog/logging.h>

extern "C" {

#define UNW_LOCAL_ONLY
#include <libunwind.h>
}

#include <signal.h>

#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>

#include "cris/core/cr_signal.h"
#include "cris/core/timer/timer.h"

namespace cris::core {

static void WriteToGlog(const char *data, int size) {
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

static void DumpSignalInfo(int signal_number, siginfo_t *siginfo) {
    constexpr size_t kBufferLen = 1024;
    char             buffer[kBufferLen];

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

static void DumpStackFrame(int level, unw_cursor_t *stack_cursor) {
    constexpr size_t kBufferLen    = 1024;
    constexpr size_t kMaxCmdLen    = 256;
    constexpr size_t kMaxSymbolLen = 256;
    constexpr size_t kFileInfoLen  = 256;
    char             buffer[kBufferLen];
    char             symbol[kMaxSymbolLen];
    char             fileinfo[kFileInfoLen];
    unw_word_t       function_offset;
    unw_word_t       pc;
    unw_word_t       sp;

    unw_get_proc_name(stack_cursor, symbol, kMaxSymbolLen, &function_offset);
    unw_get_reg(stack_cursor, UNW_REG_IP, &pc);
    unw_get_reg(stack_cursor, UNW_REG_SP, &sp);

    --pc;

    char cmd[kMaxCmdLen];

    snprintf(cmd, kMaxCmdLen, "addr2line %p -e /proc/%d/exe", reinterpret_cast<void *>(pc), getpid());

    FILE *cmd_fd = popen(cmd, "r");
    if (cmd_fd) {
        auto res = fgets(fileinfo, kFileInfoLen, cmd_fd);
        if (!res || !std::strncmp(res, "??:", 3)) {
            std::strcpy(fileinfo, "<unknown>");
        }
        pclose(cmd_fd);
    }

    snprintf(buffer, kBufferLen, "  %d: (%s+%lu) %s", level, symbol, function_offset, fileinfo);
    LOG(ERROR) << buffer;
}

static void DumpStacktrace() {
    LOG(ERROR);
    LOG(ERROR) << "******************************************************";
    LOG(ERROR) << "*******************   STACKTRACE   *******************";
    LOG(ERROR) << "******************************************************";
    LOG(ERROR);
    unw_cursor_t  stack_cursor;
    unw_context_t context;

    unw_getcontext(&context);
    unw_init_local(&stack_cursor, &context);

    int level = 0;
    while (unw_step(&stack_cursor)) {
        DumpStackFrame(++level, &stack_cursor);
    }
}

static std::atomic<pthread_t *> current_thread_in_handler{nullptr};

static void SigIntHandler(int signal_number, siginfo_t *signal_info, void *ucontext) {
    TimerSection::FlushCollectedStats();
    TimerSection::GetMainSection()->GetReport()->PrintToLog();
}

static void SignalHandler(int signal_number, siginfo_t *signal_info, void *ucontext) {
    pthread_t  current_thread            = pthread_self();
    pthread_t *expect_current_in_handler = nullptr;
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

    for (size_t i = 0; i < sizeof(kFailureSignals) / sizeof(kFailureSignals[0]); ++i) {
        CHECK_ERR(sigaction(kFailureSignals[i], &sig_action, NULL));
    }
}

void InstallSignalHandler() {
    InstallFailureSignalHandler();
    google::InstallFailureSignalHandler();
    google::InstallFailureWriter(WriteToGlog);
}

}  // namespace cris::core
