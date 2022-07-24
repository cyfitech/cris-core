#pragma once

#include "cris/core/msg/message.h"

#include <string>

namespace cris::core::impl {

std::string GetMessageRecordFileName(const std::string& message_type, const CRMessageBase::channel_subid_t subid);

}  // namespace cris::core::impl
