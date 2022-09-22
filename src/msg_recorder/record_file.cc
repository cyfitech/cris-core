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

class RecordFileKeyLdbCmp : public leveldb::Comparator {
   public:
    int Compare(const leveldb::Slice& lhs, const leveldb::Slice& rhs) const override;

    const char* Name() const override;

    // Ignore the following.
    void FindShortestSeparator(std::string*, const leveldb::Slice&) const override {}
    void FindShortSuccessor(std::string*) const override {}
};

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
    static const std::regex key_str_format("T([[:digit:]]+)ns([[:digit:]]+)");

    std::smatch key_str_match;

    CHECK(std::regex_match(str, key_str_match, key_str_format))
        << __func__ << ": Unknown key format: \"" << str << "\".";
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

int RecordFileKeyLdbCmp::Compare(const leveldb::Slice& lhs, const leveldb::Slice& rhs) const {
    return RecordFileKey::compare(RecordFileKey::FromLegacySlice(lhs), RecordFileKey::FromLegacySlice(rhs));
}

const char* RecordFileKeyLdbCmp::Name() const {
    static const auto name = GetTypeName<std::remove_cvref_t<decltype(*this)>>();
    return name.c_str();
}

RecordFileIterator::RecordFileIterator(leveldb::Iterator* db_itr) : RecordFileIterator(db_itr, false) {
}

RecordFileIterator::RecordFileIterator(leveldb::Iterator* db_itr, const bool legacy)
    : db_itr_(db_itr)
    , legacy_(legacy) {
}

bool RecordFileIterator::Valid() const {
    return db_itr_->Valid();
}

RecordFileKey RecordFileIterator::GetKey() const {
    return legacy_ ? RecordFileKey::FromLegacySlice(db_itr_->key()) : RecordFileKey::FromSlice(db_itr_->key());
}

std::pair<RecordFileKey, std::string> RecordFileIterator::Get() const {
    return std::make_pair(GetKey(), db_itr_->value().ToString());
}

void RecordFileIterator::Next() {
    db_itr_->Next();
}

RecordFile::RecordFile(std::string file_path) : file_path_(std::move(file_path)) {
    leveldb::DB*     db;
    leveldb::Options options;
    options.create_if_missing = true;

    auto status = leveldb::DB::Open(options, file_path_, &db);
    if (!status.ok()) {
        static RecordFileKeyLdbCmp legacy_cmp;
        options.comparator = &legacy_cmp;
        status             = leveldb::DB::Open(options, file_path_, &db);
        legacy_            = true;
    }
    if (!status.ok()) [[unlikely]] {
        LOG(ERROR) << __func__ << ": Failed to create record file \"" << file_path_
                   << "\", status: " << status.ToString();
    }
    db_.reset(db);
}

RecordFile::~RecordFile() {
    const auto is_empty = Empty();

    Compact();

    db_.reset();
    if (is_empty) {
        LOG(INFO) << "Record \"" << file_path_ << "\" is empty, removing.";
        std::filesystem::remove_all(file_path_);
    }
}

void RecordFile::Write(std::string serialized_value) {
    return Write(RecordFileKey::Make(), std::move(serialized_value));
}

void RecordFile::Write(RecordFileKey key, std::string serialized_value) {
    auto           key_str = key.ToBytes();
    leveldb::Slice key_slice(key_str);
    leveldb::Slice value_slice = serialized_value;
    auto           status      = db_->Put(leveldb::WriteOptions(), key_slice, value_slice);
    if (!status.ok()) [[unlikely]] {
        LOG(ERROR) << __func__ << ": Failed to write to record file \"" << file_path_
                   << "\", status: " << status.ToString();
    }
}

RecordFileIterator RecordFile::Iterate() const {
    auto itr = db_->NewIterator(leveldb::ReadOptions());
    itr->SeekToFirst();
    return RecordFileIterator(std::move(itr), legacy_);
}

bool RecordFile::Empty() const {
    return !Iterate().Valid();
}

void RecordFile::Compact() {
    db_->CompactRange(nullptr, nullptr);
}

}  // namespace cris::core
