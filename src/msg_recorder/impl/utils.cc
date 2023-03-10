#include "cris/core/msg_recorder/impl/utils.h"

#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace cris::core::impl {

std::string GetMessageRecordFileName(const std::string& message_type, const CRMessageBase::channel_subid_t subid) {
    auto       message_record_filename   = message_type;
    const auto message_type_replace_pred = [](char c) {
        return !std::isalnum(c);
    };
    std::replace_if(message_record_filename.begin(), message_record_filename.end(), message_type_replace_pred, '_');
    return message_record_filename + "_subid_" + std::to_string(subid) + ".ldb.d";
}

std::vector<fs::path> FindMatchedSubdirs(const fs::path& top_dir, const std::string_view dir_name) {
    std::vector<fs::path> matches;

    if (!fs::is_directory(top_dir)) {
        return matches;
    }

    for (const auto& entry : fs::recursive_directory_iterator{top_dir}) {
        if (fs::is_directory(entry) && entry.path().filename().native() == dir_name) {
            matches.push_back(entry.path());
        }
    }

    std::sort(std::begin(matches), std::end(matches));

    return matches;
}

}  // namespace cris::core::impl
