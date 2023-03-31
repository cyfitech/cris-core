#pragma once

#include "cris/core/msg_recorder/record_key.h"
#include "cris/core/msg_recorder/rolling_helper.h"

#include "leveldb/db.h"

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace cris::core {

class RecordFileIterator {
   public:
    explicit RecordFileIterator(leveldb::Iterator* db_itr);

    explicit RecordFileIterator(leveldb::Iterator* db_itr, const bool legacy);

    RecordFileIterator(const RecordFileIterator&) = delete;
    RecordFileIterator(RecordFileIterator&&)      = default;
    RecordFileIterator& operator=(const RecordFileIterator&) = delete;
    RecordFileIterator& operator=(RecordFileIterator&&) = default;
    ~RecordFileIterator()                               = default;

    bool Valid() const;

    RecordFileKey GetKey() const;

    std::pair<RecordFileKey, std::string> Get() const;

    void Next();

   protected:
    std::optional<RecordFileKey> TryReadCurrentKey() const;

    void ReadNextValidKey();

    std::unique_ptr<leveldb::Iterator> db_itr_;
    bool                               legacy_{false};
    RecordFileKey                      current_key_;
};

class RecordFileReverseIterator {
   public:
    explicit RecordFileReverseIterator(leveldb::Iterator* db_itr);

    explicit RecordFileReverseIterator(leveldb::Iterator* db_itr, const bool legacy);

    RecordFileReverseIterator(const RecordFileReverseIterator&) = delete;
    RecordFileReverseIterator(RecordFileReverseIterator&&)      = default;
    RecordFileReverseIterator& operator=(const RecordFileReverseIterator&) = delete;
    RecordFileReverseIterator& operator=(RecordFileReverseIterator&&) = default;
    ~RecordFileReverseIterator()                                      = default;

    bool Valid() const;

    RecordFileKey GetKey() const;

    std::pair<RecordFileKey, std::string> Get() const;

    void Prev();

   protected:
    std::optional<RecordFileKey> TryReadCurrentKey() const;

    void ReadPrevValidKey();

    std::unique_ptr<leveldb::Iterator> db_itr_;
    bool                               legacy_{false};
    RecordFileKey                      current_key_;
};

class RecordFile {
   public:
    explicit RecordFile(
        std::string                    filepath,
        std::string                    linkname       = {},
        std::unique_ptr<RollingHelper> rolling_helper = {});

    ~RecordFile();

    RecordFile(const RecordFile&) = delete;
    RecordFile(RecordFile&&)      = delete;
    RecordFile& operator=(const RecordFile&) = delete;
    RecordFile& operator=(RecordFile&&) = delete;

    void Write(std::string serialized_value);

    void Write(RecordFileKey key, std::string serialized_value);

    RecordFileIterator Iterate() const;

    RecordFileReverseIterator ReverseIterate() const;

    bool Empty() const;

    bool IsOpen() const;

    std::string GetFilePath() const;

    void Compact();

    bool OpenDB();

    void CloseDB();

    std::atomic_bool compact_before_close{false};

    static std::string UnfinishedPath(const std::string& path);

   protected:
    void CheckoutDB();

    void CommitDB();

    [[nodiscard]] leveldb::DB* OpenDB(const std::string& path);

    bool Roll();

    bool Write(const std::string& key, const std::string& value) const;

    std::filesystem::path          filepath_;
    const std::string              linkname_;
    std::unique_ptr<RollingHelper> rolling_helper_;
    std::unique_ptr<leveldb::DB>   db_;
    bool                           legacy_{false};
};

bool MakeDirs(const std::filesystem::path& path);

bool Symlink(const std::filesystem::path& to, const std::filesystem::path& from);

bool IsEmptyDir(const std::string& path);

bool IsLevelDBDir(const std::string& path);

}  // namespace cris::core
