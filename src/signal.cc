#include <glog/logging.h>

#include <string>

namespace cris::core {

static void WriteToGlog(const char *data, int size) {
    std::string msg(data, size);
    LOG(ERROR) << msg;
}

void InstallSignalHandler() {
    google::InstallFailureSignalHandler();
    google::InstallFailureWriter(WriteToGlog);
}

}  // namespace cris::core
