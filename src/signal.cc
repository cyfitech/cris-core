#include <glog/logging.h>

namespace cris::core {

void InstallSignalHandler() {
    google::InstallFailureSignalHandler();
}

}  // namespace cris::core
