#pragma once

#include "cris/core/msg/message.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace cris::core::impl {

std::string GetMessageRecordFileName(const std::string& message_type, const CRMessageBase::channel_subid_t subid);

std::vector<std::filesystem::path> FindMatchedSubdirs(
    const std::filesystem::path& top_dir,
    const std::string_view       dir_name);

}  // namespace cris::core::impl
