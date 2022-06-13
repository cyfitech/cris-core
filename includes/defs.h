#pragma once

#include <string>

namespace cris::core::impl {

std::string GetTypeNameFromPrettyFunction(const std::string& pretty_function);

}  // namespace cris::core::impl

namespace {

// This function cannot be in any named namespace, otherwise the namespace prefix of T may be dismissed.
template<class T>
std::string GetTypeNameImpl() {
    static const std::string name = cris::core::impl::GetTypeNameFromPrettyFunction(__PRETTY_FUNCTION__);
    return name;
}

}  // namespace

namespace cris::core {

// Get the human-friendly type name
template<class T>
std::string GetTypeName() {
    return GetTypeNameImpl<T>();
}

}  // namespace cris::core
