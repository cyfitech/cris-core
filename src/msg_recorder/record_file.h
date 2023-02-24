#pragma once

#include "cris/core/msg_recorder/record_key.h"

#include "leveldb/db.h"

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

class RecordFile {
   public:
    explicit RecordFile(std::string file_path);

    ~RecordFile();

    RecordFile(const RecordFile&) = delete;
    RecordFile(RecordFile&&)      = default;
    RecordFile& operator=(const RecordFile&) = delete;
    RecordFile& operator=(RecordFile&&) = default;

    void Write(std::string serialized_value);

    void Write(RecordFileKey key, std::string serialized_value);

    RecordFileIterator Iterate() const;

    bool Empty() const;

    bool isOpen();

    void Compact();

    bool OpenDB();

    void CloseDB();

   protected:
    std::string                  file_path_;
    std::unique_ptr<leveldb::DB> db_;
    bool                         legacy_{false};
};

}  // namespace cris::core
