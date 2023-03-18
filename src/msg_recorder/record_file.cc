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

namespace fs = std::filesystem;

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

RecordFileReverseIterator::RecordFileReverseIterator(leveldb::Iterator* db_itr)
    : RecordFileReverseIterator(db_itr, false) {
}

RecordFileReverseIterator::RecordFileReverseIterator(leveldb::Iterator* db_itr, const bool legacy)
    : db_itr_(db_itr)
    , legacy_(legacy) {
    ReadPrevValidKey();
}

bool RecordFileReverseIterator::Valid() const {
    return db_itr_->Valid();
}

std::optional<RecordFileKey> RecordFileReverseIterator::TryReadCurrentKey() const {
    auto key_slice = db_itr_->key();
    auto key_bytes = std::string_view(key_slice.data(), key_slice.size());
    return legacy_ ? RecordFileKey::FromBytesLegacy(key_bytes) : RecordFileKey::FromBytes(key_bytes);
}

RecordFileKey RecordFileReverseIterator::GetKey() const {
    return current_key_;
}

std::pair<RecordFileKey, std::string> RecordFileReverseIterator::Get() const {
    return std::make_pair(GetKey(), db_itr_->value().ToString());
}

void RecordFileReverseIterator::Prev() {
    db_itr_->Prev();
    ReadPrevValidKey();
}

void RecordFileReverseIterator::ReadPrevValidKey() {
    for (; db_itr_->Valid(); db_itr_->Prev()) {
        auto key_opt = TryReadCurrentKey();
        if (!key_opt) {
            continue;
        }
        current_key_ = *key_opt;
        break;
    }
}

RecordFile::RecordFile(std::string filepath, std::string linkname, std::unique_ptr<RollingHelper> rolling_helper)
    : filepath_{std::move(filepath)}
    , filename_{fs::path{filepath_}.filename().native()}
    , linkname_{std::move(linkname)}
    , rolling_helper_{std::move(rolling_helper)} {
    OpenDB();
}

RecordFile::~RecordFile() {
    const auto is_empty = Empty();
    CloseDB();
    if (is_empty) {
        if (!linkname_.empty()) {
            const auto symlink_path = fs::path{filepath_}.parent_path() / linkname_;
            fs::remove(symlink_path);
        }

        LOG(INFO) << "Record \"" << filepath_ << "\" is empty, removing.";
        fs::remove_all(filepath_);
    }
}

bool RecordFile::OpenDB() {
    if (db_) {
        LOG(ERROR) << __func__ << ": Already has another DB opened.";
        return false;
    }

    if (filepath_.empty()) {
        LOG(ERROR) << __func__ << ": Failed to open the database, the file path is empty.";
        return false;
    }

    leveldb::DB* db = OpenDB(filepath_);
    if (db == nullptr) {
        return false;
    }

    db_.reset(db);
    return true;
}

leveldb::DB* RecordFile::OpenDB(const std::string& path) {
    const fs::path filepath{path};
    const fs::path dir{filepath.parent_path()};
    if (!MakeDirs(dir)) {
        LOG(ERROR) << __func__ << ": Failed to OpenDB(), failed to create dirs.";
        return nullptr;
    }

    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::DB* db     = nullptr;
    auto         status = leveldb::DB::Open(options, path, &db);
    if (!status.ok()) {
        static RecordFileKeyLdbCmp legacy_cmp;
        options.comparator = &legacy_cmp;
        status             = leveldb::DB::Open(options, path, &db);
        legacy_            = true;
    }
    if (!status.ok()) [[unlikely]] {
        LOG(ERROR) << __func__ << ": Failed to create record file " << filepath << ", status: " << status.ToString();
        return nullptr;
    }

    const auto symlink_path = dir / linkname_;
    if (!fs::exists(symlink_path)) {
        Symlink(filepath, symlink_path);
    }

    return db;
}

void RecordFile::CloseDB() {
    if (!db_) {
        LOG(ERROR) << __func__ << ": The database has already been closed elsewhere.";
        return;
    }

    if (compact_before_close) {
        Compact();
    }

    db_.reset();
}

void RecordFile::Write(std::string serialized_value) {
    return Write(RecordFileKey::Make(), std::move(serialized_value));
}

void RecordFile::Write(RecordFileKey key, std::string serialized_value) {
    const auto key_str = key.ToBytes();

    const RollingHelper::Metadata metadata{
        .time       = std::chrono::system_clock::now(),
        .value_size = serialized_value.size()};
    if (!rolling_helper_) {
        Write(key_str, serialized_value);
        return;
    }

    if (!rolling_helper_->NeedToRoll(metadata) || Roll()) {
        Write(key_str, serialized_value);
        rolling_helper_->Update(metadata);
        return;
    }

    LOG(ERROR) << __func__ << ": Failed to roll records, fallback to current db.";
    Write(key_str, serialized_value);
}

bool RecordFile::Roll() {
    const auto path     = rolling_helper_->GenerateFullRecordDirPath() / filename_;
    auto       path_str = path.native();

    decltype(db_) new_db{OpenDB(path_str)};
    if (new_db == nullptr) {
        LOG(ERROR) << __func__ << ": Failed to open new db for rolling, path " << path;
        return false;
    }

    CloseDB();
    db_       = std::move(new_db);
    filepath_ = std::move(path_str);

    rolling_helper_->Reset();

    return true;
}

void RecordFile::Write(const std::string& key, const std::string& value) const {
    leveldb::Slice key_slice{key};
    leveldb::Slice value_slice{value};
    auto           status = db_->Put(leveldb::WriteOptions(), key_slice, value_slice);
    if (!status.ok()) [[unlikely]] {
        LOG(ERROR) << __func__ << ": Failed to write to record file \"" << filepath_
                   << "\", status: " << status.ToString();
    }
}

RecordFileIterator RecordFile::Iterate() const {
    auto* itr = db_->NewIterator(leveldb::ReadOptions());
    itr->SeekToFirst();
    return RecordFileIterator(itr, legacy_);
}

RecordFileReverseIterator RecordFile::ReverseIterate() const {
    auto* itr = db_->NewIterator(leveldb::ReadOptions());
    itr->SeekToLast();
    return RecordFileReverseIterator(itr, legacy_);
}

bool RecordFile::IsOpen() const {
    return db_ != nullptr;
}

bool RecordFile::Empty() const {
    return !Iterate().Valid();
}

const std::string& RecordFile::GetFilePath() const {
    return filepath_;
}

void RecordFile::Compact() {
    db_->CompactRange(nullptr, nullptr);
}

bool MakeDirs(const std::filesystem::path& path) {
    std::error_code ec{};
    if (!fs::create_directories(path, ec) && ec) {
        LOG(ERROR) << __func__ << ": Failed to create dir " << path << "with error \"" << ec.message() << "\".";
        return false;
    }

    return true;
}

bool Symlink(const std::filesystem::path& to, const std::filesystem::path& from) {
    if (from.empty()) {
        LOG(WARNING) << __func__ << ": Failed to create symlink since empty link name provided.";
        return false;
    }

    std::error_code ec{};
    fs::create_symlink(to, from, ec);
    const bool has_error = ec.operator bool();
    LOG_IF(WARNING, has_error) << __func__ << ": Failed to create symlink to " << to << " with error \"" << ec.message()
                               << "\".";
    return has_error;
}

}  // namespace cris::core
