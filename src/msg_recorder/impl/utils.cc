#include "cris/core/msg_recorder/impl/utils.h"

#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace cris::core::impl {

std::string GetMessageFileName(const std::string& message_type) {
    auto       message_typename = message_type;
    const auto replace_pred     = [](char c) {
        return !std::isalnum(c);
    };
    std::replace_if(message_typename.begin(), message_typename.end(), replace_pred, '_');
    return message_typename;
}

std::vector<fs::path> ListSubdirsWithSuffix(const fs::path& top_dir, const std::string_view suffix) {
    std::vector<fs::path> matches;

    if (!fs::is_directory(top_dir)) {
        return matches;
    }

    for (const auto& entry : fs::directory_iterator{top_dir}) {
        if (fs::is_directory(entry) && entry.path().filename().native().ends_with(suffix)) {
            matches.push_back(entry.path());
        }
    }

    std::sort(std::begin(matches), std::end(matches));

    return matches;
}

}  // namespace cris::core::impl
