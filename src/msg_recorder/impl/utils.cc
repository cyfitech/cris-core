#include "utils.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>

namespace cris::core::impl {

std::string GetMessageRecordFileName(const std::string& message_type, const CRMessageBase::channel_subid_t subid) {
    auto       message_record_filename   = message_type;
    const auto message_type_replace_pred = [](char c) {
        return !std::isalnum(c);
    };
    std::replace_if(message_record_filename.begin(), message_record_filename.end(), message_type_replace_pred, '_');
    return message_record_filename + "_subid_" + std::to_string(subid) + ".ldb";
}

}  // namespace cris::core::impl
