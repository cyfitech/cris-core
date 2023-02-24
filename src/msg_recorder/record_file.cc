#include "cris/core/msg_recorder/record_file.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/slice.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace cris::core {

class RecordFileKeyLdbCmp : public leveldb::Comparator {
   public:
    int Compare(const leveldb::Slice& lhs, const leveldb::Slice& rhs) const override;

    const char* Name() const override;

    // Ignore the following.
    void FindShortestSeparator(std::string* /* start */, const leveldb::Slice& /* limit */) const override {}
    void FindShortSuccessor(std::string* /* key */) const override {}
};

int RecordFileKeyLdbCmp::Compare(const leveldb::Slice& lhs, const leveldb::Slice& rhs) const {
    auto lhs_key_opt = RecordFileKey::FromBytesLegacy(std::string_view(lhs.data(), lhs.size()));
    auto rhs_key_opt = RecordFileKey::FromBytesLegacy(std::string_view(rhs.data(), rhs.size()));

    if (lhs_key_opt && rhs_key_opt) {
        return RecordFileKey::compare(*lhs_key_opt, *rhs_key_opt);
    } else if (lhs_key_opt) {
        return 1;
    } else if (rhs_key_opt) {
        return -1;
    }
    return 0;
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
    db_itr_->SeekToFirst();
    ReadNextValidKey();
}

bool RecordFileIterator::Valid() const {
    return db_itr_->Valid();
}

std::optional<RecordFileKey> RecordFileIterator::TryReadCurrentKey() const {
    auto key_slice = db_itr_->key();
    auto key_bytes = std::string_view(key_slice.data(), key_slice.size());
    return legacy_ ? RecordFileKey::FromBytesLegacy(key_bytes) : RecordFileKey::FromBytes(key_bytes);
}

RecordFileKey RecordFileIterator::GetKey() const {
    return current_key_;
}

std::pair<RecordFileKey, std::string> RecordFileIterator::Get() const {
    return std::make_pair(GetKey(), db_itr_->value().ToString());
}

void RecordFileIterator::Next() {
    db_itr_->Next();
    ReadNextValidKey();
}

void RecordFileIterator::ReadNextValidKey() {
    for (; db_itr_->Valid(); db_itr_->Next()) {
        auto key_opt = TryReadCurrentKey();
        if (!key_opt) {
            continue;
        }
        current_key_ = *key_opt;
        break;
    }
}

RecordFile::RecordFile(std::string file_path) : file_path_(std::move(file_path)) {
    OpenDB();
}

RecordFile::~RecordFile() {
    const auto is_empty = Empty();
    CloseDB();
    if (is_empty) {
        LOG(INFO) << "Record \"" << file_path_ << "\" is empty, removing.";
        std::filesystem::remove_all(file_path_);
    }
}

bool RecordFile::OpenDB() {
    if (db_) {
        LOG(ERROR) << __func__ << ": Already has another DB opened.";
        return false;
    }

    if (file_path_.empty()) {
        LOG(ERROR) << __func__ << ": Failed to open the database, the file path is empty.";
        return false;
    }

    leveldb::DB*     db = nullptr;
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
        return false;
    }
    db_.reset(db);
    return true;
}

void RecordFile::CloseDB() {
    if (!db_) {
        LOG(ERROR) << __func__ << ": The database has already been closed elsewhere.";
        return;
    }
    Compact();
    db_.reset();
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
    auto* itr = db_->NewIterator(leveldb::ReadOptions());
    itr->SeekToFirst();
    return RecordFileIterator(itr, legacy_);
}

bool RecordFile::IsOpen() const {
    return db_ != nullptr;
}

bool RecordFile::Empty() const {
    return !Iterate().Valid();
}

void RecordFile::Compact() {
    db_->CompactRange(nullptr, nullptr);
}

}  // namespace cris::core
