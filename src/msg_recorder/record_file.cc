#include "cris/core/msg_recorder/record_file.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"
#include "cris/core/utils/time.h"

#include "leveldb/comparator.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <memory>
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
        .timestamp_ = now,
        .count_     = global_counters[now & (kNumOfCounters - 1)].fetch_add(1),
    };
}

RecordFileKey RecordFileKey::FromSlice(const leveldb::Slice& slice) {
    RecordFileKey key;
    std::memcpy(&key, slice.data(), std::min(sizeof(key), slice.size()));
    return key;
}

int RecordFileKey::compare(const RecordFileKey& lhs, const RecordFileKey& rhs) {
    if (lhs.timestamp_ != rhs.timestamp_) {
        return lhs.timestamp_ < rhs.timestamp_ ? -1 : 1;
    } else if (lhs.count_ != rhs.count_) {
        return lhs.count_ < rhs.count_ ? -1 : 1;
    } else {
        return 0;
    }
}

int RecordFileKeyLdbCmp::Compare(const leveldb::Slice& lhs, const leveldb::Slice& rhs) const {
    return RecordFileKey::compare(RecordFileKey::FromSlice(lhs), RecordFileKey::FromSlice(rhs));
}

const char* RecordFileKeyLdbCmp::Name() const {
    static const auto name = GetTypeName<std::remove_cvref_t<decltype(*this)>>();
    return name.c_str();
}

RecordFileIterator::RecordFileIterator(leveldb::Iterator* db_itr) : db_itr_(db_itr) {
}

bool RecordFileIterator::Valid() const {
    return db_itr_->Valid();
}

RecordFileKey RecordFileIterator::GetKey() const {
    return RecordFileKey::FromSlice(db_itr_->key());
}

std::pair<RecordFileKey, std::string> RecordFileIterator::Get() const {
    return std::make_pair(GetKey(), db_itr_->value().ToString());
}

void RecordFileIterator::Next() {
    db_itr_->Next();
}

RecordFile::RecordFile(std::string file_path) : file_path_(std::move(file_path)) {
    static RecordFileKeyLdbCmp leveldb_cmp_;
    leveldb::DB*               db;
    leveldb::Options           options;
    options.create_if_missing = true;
    options.comparator        = &leveldb_cmp_;
    auto status               = leveldb::DB::Open(options, file_path_, &db);
    if (!status.ok()) [[unlikely]] {
        LOG(ERROR) << __func__ << ": Failed to create record file \"" << file_path_
                   << "\", status: " << status.ToString();
    }
    db_.reset(db);
}

RecordFile::~RecordFile() {
    const auto is_empty = Empty();
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
    leveldb::Slice key_slice(reinterpret_cast<const char*>(&key), sizeof(key));
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
    return RecordFileIterator(std::move(itr));
}

bool RecordFile::Empty() const {
    return !Iterate().Valid();
}

}  // namespace cris::core
