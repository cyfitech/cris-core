#pragma once

#include "cris/core/msg/message.h"
#include "cris/core/utils/time.h"

#include "leveldb/comparator.h"
#include "leveldb/db.h"

#include <memory>
#include <string>
#include <utility>

namespace cris::core {

struct RecordFileKey {
    static RecordFileKey Make();

    static RecordFileKey FromSlice(const leveldb::Slice& slice);

    static int compare(const RecordFileKey& lhs, const RecordFileKey& rhs);

    class LevelDBComparator : public leveldb::Comparator {
       public:
        int Compare(const leveldb::Slice& lhs, const leveldb::Slice& rhs) const override;

        const char* Name() const override;

        // Ignore the following.
        void FindShortestSeparator(std::string*, const leveldb::Slice&) const override {}
        void FindShortSuccessor(std::string*) const override {}
    };

    // Use timestamp as primary for easier db merging and cross-db comparison.
    cr_timestamp_nsec_t timestamp_{0};

    // tie-breaker if timestamp happends to be the same.
    unsigned long long count_{0};

    // If there are more fields needed, add them below for backward-compatibilitty.
};

static_assert(std::is_standard_layout_v<RecordFileKey>);

class RecordFileIterator {
   public:
    explicit RecordFileIterator(leveldb::Iterator* db_itr);

    RecordFileIterator(const RecordFileIterator&) = delete;
    RecordFileIterator(RecordFileIterator&&)      = default;
    RecordFileIterator& operator=(const RecordFileIterator&) = delete;
    RecordFileIterator& operator=(RecordFileIterator&&) = default;
    ~RecordFileIterator()                               = default;

    bool Valid() const;

    std::pair<RecordFileKey, std::string> Get() const;

    RecordFileKey GetKey() const;

    void Next();

   protected:
    std::unique_ptr<leveldb::Iterator> db_itr_;
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

   protected:
    std::string                      file_path_;
    std::unique_ptr<leveldb::DB>     db_;
    RecordFileKey::LevelDBComparator leveldb_cmp_;
};

template<CRMessageType message_t>
void MessageFromStr(message_t& msg, const std::string& serialized_msg);

template<CRMessageType message_t>
std::string MessageToStr(const message_t& msg);

}  // namespace cris::core
