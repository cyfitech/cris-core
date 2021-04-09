#include <glog/logging.h>

namespace cris::core {

namespace impl {

static int InstallFailureSignalHandler() {
    google::InstallFailureSignalHandler();
    return 0;
}

[[maybe_unused]] const int kInstallFailureSignalHandler = InstallFailureSignalHandler();

}  // namespace impl

}  // namespace cris::core
