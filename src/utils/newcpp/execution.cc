#include "cris/core/utils/newcpp/execution.h"

#include "cris/core/utils/logging.h"

#include <execution>

namespace cris::libs::execution {

namespace impl {

void PrintFallbackWarningOnce() {
    [[maybe_unused]] static int print_once = []() {
        LOG(WARNING) << "The specified execution policy is not available. Falling back to sequential execution.";
        return 0;
    }();
}

}  // namespace impl

}  // namespace cris::libs::execution
