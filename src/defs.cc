#include "defs.h"

namespace cris::core {

namespace impl {

std::string GetTypeNameFromPrettyFunction(const std::string &pretty_function) {
    const std::string start_pattern = "T = ";
    auto              start_pos     = pretty_function.find(start_pattern) + start_pattern.length();
    auto              end_pos       = pretty_function.find(";");
    if (end_pos == std::string::npos) {
        end_pos = pretty_function.find("]");
    }
    return pretty_function.substr(start_pos, end_pos - start_pos);
}

}  // namespace impl

}  // namespace cris::core
