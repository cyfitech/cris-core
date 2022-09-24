#include "cris/core/msg_recorder/record_file.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"
#include "cris/core/utils/time.h"

#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/slice.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

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

RecordFileKey RecordFileKey::FromBytes(const std::string& str) {
    static thread_local const std::regex key_str_format("T([[:digit:]]+)ns([[:digit:]]+)");

    std::smatch key_str_match;

    CHECK(std::regex_match(str, key_str_match, key_str_format))
        << __func__ << ": Unknown key format \"" << str << "\".";
    CHECK_GT(key_str_match.size(), 2u);

    return RecordFileKey{
        .timestamp_ns_ = std::stoll(key_str_match[1].str(), nullptr),
        .count_        = std::stoull(key_str_match[2].str(), nullptr),
    };
}

RecordFileKey RecordFileKey::FromSlice(const leveldb::Slice& slice) {
    return FromBytes(slice.ToString());
}

RecordFileKey RecordFileKey::FromLegacySlice(const leveldb::Slice& slice) {
    RecordFileKey key;
    std::memcpy(&key, slice.data(), std::min(sizeof(key), slice.size()));
    return key;
}

int RecordFileKey::compare(const RecordFileKey& lhs, const RecordFileKey& rhs) {
    auto lhs_str = lhs.ToBytes();
    auto rhs_str = rhs.ToBytes();
    return leveldb::BytewiseComparator()->Compare(leveldb::Slice(lhs_str), leveldb::Slice(rhs_str));
}

}  // namespace cris::core
