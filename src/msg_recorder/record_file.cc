#include "cris/core/msg_recorder/record_file.h"

#include "cris/core/msg_recorder/impl/utils.h"
#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/slice.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

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
    , linkname_{std::move(linkname)}
    , rolling_helper_{std::move(rolling_helper)} {
    OpenDB();
}

RecordFile::~RecordFile() {
    CloseDB();
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

    CheckoutDB();

    leveldb::DB* db = OpenDB(filepath_.native());
    if (db == nullptr) {
        return false;
    }

    db_.reset(db);

    if (!linkname_.empty()) {
        const auto subchannel_dir = filepath_.parent_path();
        const auto message_dir    = subchannel_dir.parent_path();
        const auto linkpath       = message_dir.parent_path() / linkname_;
        if (!fs::exists(linkpath)) {
            Symlink(message_dir.filename() / subchannel_dir.filename(), linkpath);
        }
    }
    return true;
}

leveldb::DB* RecordFile::OpenDB(const std::string& path) {
    const auto     actual_filepath = UnfinishedPath(path);
    const fs::path filepath{actual_filepath};
    const fs::path dir{filepath.parent_path()};
    if (!MakeDirs(dir)) {
        LOG(ERROR) << __func__ << ": Failed to OpenDB(), failed to create dir " << dir << ".";
        return nullptr;
    }

    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::DB* db     = nullptr;
    auto         status = leveldb::DB::Open(options, actual_filepath, &db);
    if (!status.ok()) {
        static RecordFileKeyLdbCmp legacy_cmp;
        options.comparator = &legacy_cmp;
        status             = leveldb::DB::Open(options, actual_filepath, &db);
        legacy_            = true;
    }
    if (!status.ok()) [[unlikely]] {
        LOG(ERROR) << __func__ << ": Failed to create record file " << filepath << ", status: " << status.ToString();
        return nullptr;
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

    const bool is_empty = Empty();

    db_.reset();
    CommitDB();

    if ((is_empty || IsEmptyDir(filepath_.native())) && IsLevelDBDir(filepath_.native())) {
        LOG(INFO) << "Remove empty record DB " << filepath_ << ".";
        fs::remove_all(filepath_);
    }
}

void RecordFile::CheckoutDB() {
    if (!fs::exists(filepath_)) {
        return;
    }

    const fs::path actual_filepath{UnfinishedPath(filepath_.native())};
    if (fs::exists(actual_filepath)) {
        LOG(ERROR) << __func__ << ": Failed to checkout db, dir " << actual_filepath << " already exists.";
        throw std::runtime_error{actual_filepath.native() + " already exists."};
    }

    try {
        fs::rename(filepath_, actual_filepath);
    } catch (const fs::filesystem_error& e) {
        LOG(ERROR) << __func__ << ": Failed to rename " << filepath_ << " to " << actual_filepath << " with error "
                   << std::quoted(e.what()) << ".";
        throw;
    }
}

void RecordFile::CommitDB() {
    const fs::path actual_filepath{UnfinishedPath(filepath_.native())};

    if (!fs::exists(actual_filepath)) {
        LOG(ERROR) << __func__ << ": Failed to commit db, dir " << actual_filepath << " does not exist.";
        throw std::runtime_error{actual_filepath.native() + " does not exists."};
    }

    if (fs::exists(filepath_)) {
        LOG(ERROR) << __func__ << ": Failed to commit db, dir " << filepath_ << " already exists.";
        throw std::runtime_error{filepath_.native() + " already exists."};
    }

    try {
        fs::rename(actual_filepath, filepath_);
    } catch (const fs::filesystem_error& e) {
        LOG(ERROR) << __func__ << ": Failed to rename " << actual_filepath << " to " << filepath_ << " with error "
                   << std::quoted(e.what()) << ".";
        throw;
    }
}

std::string RecordFile::UnfinishedPath(const std::string& path_str) {
    return path_str + ".saving";
}

void RecordFile::Write(std::string serialized_value) {
    return Write(RecordFileKey::Make(), std::move(serialized_value));
}

void RecordFile::Write(RecordFileKey key, std::string serialized_value) {
    const auto key_str = key.ToBytes();

    const RollingHelper::Metadata metadata{
        .time       = std::chrono::system_clock::now(),
        .value_size = serialized_value.size()};

    if (rolling_helper_ && rolling_helper_->NeedToRoll(metadata) && !Roll()) {
        LOG(ERROR) << __func__ << ": Failed to roll records, fallback to current db.";
    }

    if (Write(key_str, serialized_value) && rolling_helper_) {
        rolling_helper_->Update(metadata);
    }
}

bool RecordFile::Roll() {
    const fs::path new_filepath{filepath_.parent_path() / rolling_helper_->MakeNewRecordDirName()};
    decltype(db_)  new_db{OpenDB(new_filepath.native())};
    if (new_db == nullptr) {
        LOG(ERROR) << __func__ << ": Failed to open new db " << filepath_ << " for rolling.";
        return false;
    }

    CloseDB();
    db_       = std::move(new_db);
    filepath_ = new_filepath;

    rolling_helper_->Reset();

    return true;
}

bool RecordFile::Write(const std::string& key, const std::string& value) const {
    leveldb::Slice key_slice{key};
    leveldb::Slice value_slice{value};
    const auto     status = db_->Put(leveldb::WriteOptions(), key_slice, value_slice);
    const bool     ok     = status.ok();
    if (!ok) [[unlikely]] {
        LOG(ERROR) << __func__ << ": Failed to write to record file " << filepath_ << ", status: " << status.ToString();
    }
    return ok;
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

std::string RecordFile::GetFilePath() const {
    return IsOpen() ? UnfinishedPath(filepath_.string()) : filepath_.string();
}

void RecordFile::Compact() {
    db_->CompactRange(nullptr, nullptr);
}

bool MakeDirs(const std::filesystem::path& path) {
    std::error_code ec{};
    if (!fs::create_directories(path, ec) && ec) {
        LOG(ERROR) << __func__ << ": Failed to create dir " << path << "with error " << std::quoted(ec.message())
                   << ".";
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
    LOG_IF(WARNING, has_error) << __func__ << ": Failed to create symlink from " << from << " -> " << to
                               << " with error " << std::quoted(ec.message()) << ".";
    return has_error;
}

bool IsEmptyDir(const std::string& dir_path) {
    const fs::path dir{dir_path};
    return fs::is_directory(dir) && fs::is_empty(dir);
}

bool IsLevelDBDir(const std::string& dir_path) {
    const fs::path dir{dir_path};
    if (!fs::exists(dir / "CURRENT")) {
        return false;
    }

    const auto dir_iter = fs::directory_iterator{dir};
    return std::any_of(fs::begin(dir_iter), fs::end(dir_iter), [](const auto& entry) {
        constexpr std::string_view kLevelDBManifestPrefix{"MANIFEST-"};
        return fs::is_regular_file(entry) && entry.path().filename().native().starts_with(kLevelDBManifestPrefix);
    });
}

std::vector<fs::path> ListLevelDBDirs(
    const std::string&  dir_path,
    const std::string&  msg_type,
    const std::uint64_t subid) {
    const auto msg_name       = impl::GetMessageFileName(msg_type);
    const auto subchannel_dir = fs::path{dir_path} / msg_name / std::to_string(subid);
    return impl::ListSubdirsWithSuffix(subchannel_dir, impl::kLevelDBDirSuffix);
}

}  // namespace cris::core
