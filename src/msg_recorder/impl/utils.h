#pragma once

#include "cris/core/msg/message.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace cris::core::impl {

constexpr std::string_view kLevelDbDirSuffix = ".ldb.d";

std::string GetMessageFileName(const std::string& message_type);

std::vector<std::filesystem::path> ListSubdirsWithSuffix(
    const std::filesystem::path& top_dir,
    const std::string_view       suffix);

}  // namespace cris::core::impl
