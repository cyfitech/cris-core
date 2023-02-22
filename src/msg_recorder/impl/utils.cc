#include "cris/core/msg_recorder/impl/utils.h"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <set>
#include <string>

namespace cris::core::impl {

std::string GetMessageRecordFileName(const std::string& message_type, const CRMessageBase::channel_subid_t subid) {
    auto       message_record_filename   = message_type;
    const auto message_type_replace_pred = [](char c) {
        return !std::isalnum(c);
    };
    std::replace_if(message_record_filename.begin(), message_record_filename.end(), message_type_replace_pred, '_');
    return message_record_filename + "_subid_" + std::to_string(subid) + ".ldb.d";
}

const std::string& GetHostName() {
    static const std::string hostname{[] {
        char buf[HOST_NAME_MAX + 1];
        if (gethostname(buf, sizeof(buf)) == 0) {
            buf[sizeof(buf) - 1] = '\0';
            return std::string{buf};
        }

        return std::string{};
    }()};

    return hostname;
}

}  // namespace cris::core::impl
