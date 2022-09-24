#pragma once

#include "cris/core/utils/time.h"

#include <string>
#include <string_view>
#include <type_traits>

namespace cris::core {

struct RecordFileKey {
    std::string ToBytes() const;

    static RecordFileKey Make();

    static RecordFileKey FromBytes(const std::string_view bytes);

    static RecordFileKey FromBytesLegacy(const std::string_view bytes);

    static int compare(const RecordFileKey& lhs, const RecordFileKey& rhs);

    // Use timestamp as primary for easier db merging and cross-db comparison.
    cr_timestamp_nsec_t timestamp_ns_{0};

    // tie-breaker if timestamp happends to be the same.
    unsigned long long count_{0};

    // If there are more fields needed, add them below for backward-compatibilitty.
};

static_assert(std::is_standard_layout_v<RecordFileKey>);

}  // namespace cris::core
