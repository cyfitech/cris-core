#pragma once

#include <string>

namespace cris::core {

namespace impl {

std::string GetTypeNameFromPrettyFunction(const std::string& pretty_function);

}  // namespace impl

// Get the human-friendly type name
template<class T>
std::string GetTypeName() {
    static const std::string name = impl::GetTypeNameFromPrettyFunction(__PRETTY_FUNCTION__);
    return name;
}

}  // namespace cris::core
