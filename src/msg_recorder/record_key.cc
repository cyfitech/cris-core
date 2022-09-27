#include "cris/core/msg_recorder/record_key.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"
#include "cris/core/utils/time.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <regex>
#include <sstream>

namespace cris::core {

RecordFileKey RecordFileKey::Make() {
    // Those counters are the tie-breakers if the timestamps are the same
    constexpr unsigned                                                 kNumOfCounters = 1 << 4;
    static std::array<std::atomic<unsigned long long>, kNumOfCounters> global_counters{};

    const auto now = GetUnixTimestampNsec();

    static_assert(
        kNumOfCounters == (kNumOfCounters & -kNumOfCounters),
        "kNumOfCounters must be power of 2 so that we can use bitwise-and to replace modulo.");

    return {
        .timestamp_ns_ = now,
        .count_        = global_counters[now & (kNumOfCounters - 1)].fetch_add(1),
    };
}

std::string RecordFileKey::ToBytes() const {
    const int kMaxInt64Digits = static_cast<int>(std::ceil(std::log10(std::numeric_limits<std::uint64_t>::max())));

    std::ostringstream ss;
    ss << "T" << std::setfill('0') << std::setw(kMaxInt64Digits)
       << std::max(timestamp_ns_, static_cast<decltype(timestamp_ns_)>(0))  // just in case if timestamp is negative.
       << "ns" << std::setfill('0') << std::setw(kMaxInt64Digits) << count_;
    return ss.str();
}

RecordFileKey RecordFileKey::FromBytes(const std::string_view bytes) {
    static thread_local const std::regex key_str_format("T([[:digit:]]+)ns([[:digit:]]+)");

    std::smatch key_str_match;

    std::string bytes_str(bytes);

    CHECK(std::regex_match(bytes_str, key_str_match, key_str_format))
        << __func__ << ": Unknown key format \"" << bytes_str << "\".";
    CHECK_GT(key_str_match.size(), 2u);

    return RecordFileKey{
        .timestamp_ns_ = std::stoll(key_str_match[1].str(), nullptr),
        .count_        = std::stoull(key_str_match[2].str(), nullptr),
    };
}

RecordFileKey RecordFileKey::FromBytesLegacy(const std::string_view bytes) {
    RecordFileKey key;
    std::memcpy(&key, bytes.data(), std::min(sizeof(key), bytes.size()));
    return key;
}

int RecordFileKey::compare(const RecordFileKey& lhs, const RecordFileKey& rhs) {
    auto lhs_bytes = lhs.ToBytes();
    auto rhs_bytes = rhs.ToBytes();
    if (const auto cmp =
            std::memcmp(lhs_bytes.data(), rhs_bytes.data(), std::min(lhs_bytes.size(), rhs_bytes.size()))) {
        return cmp;
    } else if (lhs_bytes.size() < rhs_bytes.size()) {
        return -1;
    } else if (lhs_bytes.size() > rhs_bytes.size()) {
        return 1;
    }
    return 0;
}

}  // namespace cris::core
