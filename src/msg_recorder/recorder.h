#pragma once

#include "cris/core/msg/message.h"
#include "cris/core/msg/node.h"
#include "cris/core/msg_recorder/record_file.h"

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace cris::core {

class MessageRecorder : public CRNamedNode<MessageRecorder> {
   public:
    class SnapshotTimer {
        bool clear = false;

       public:
        template<typename Function>
        void SetSnapshotInterval(Function function, int interval);

        void SnapshotStop();
    };

    using Base = CRNamedNode<MessageRecorder>;

    explicit MessageRecorder(const std::filesystem::path& record_dir_prefix, std::shared_ptr<JobRunner> runner);

    MessageRecorder(const MessageRecorder&) = delete;
    MessageRecorder(MessageRecorder&&)      = default;
    MessageRecorder& operator=(const MessageRecorder&) = delete;
    MessageRecorder& operator=(MessageRecorder&&) = delete;

    ~MessageRecorder();

    template<CRMessageType message_t>
    void RegisterChannel(const channel_subid_t subid, const std::string& alias = "");

    std::filesystem::path GetRecordDir() const;

   private:
    using msg_serializer = std::function<std::string(const CRMessageBasePtr&)>;

    RecordFile* CreateFile(const std::string& message_type, const channel_subid_t subid, const std::string& alias);

    // snapshot interval with hour being the base unit
    enum class SnapshotInterval : std::size_t { HOURLY = 1, DAILY = 24, WEEKLY = 168 };

    // snapshot num saved for each frequency
    enum class MaxCopyNum : std::size_t { HOURLY = 48, DAILY = 14, WEEKLY = 7 };

    std::map<SnapshotInterval, MaxCopyNum> snapshot_records_{
        {SnapshotInterval::HOURLY, MaxCopyNum::HOURLY},
        {SnapshotInterval::DAILY, MaxCopyNum::DAILY},
        {SnapshotInterval::WEEKLY, MaxCopyNum::WEEKLY},
    };

    void GenerateSnapshot(const MaxCopyNum& keep_max);

    static std::string RecordDirNameGenerator();
    static std::string SnapshotDirNameGenerator();

    void SnapshotStart();
    void SnapshotEnd();

    const std::filesystem::path              record_dir_;
    std::vector<std::unique_ptr<RecordFile>> files_;

    std::shared_ptr<cris::core::JobRunnerStrand> subscription_strand_;
    SnapshotTimer                                timer;

    struct RecordFileInitData {
        std::string     message_type_name;
        channel_subid_t subid;
        std::string     alias;
    };

    std::vector<RecordFileInitData> record_init_map;
};

template<CRMessageType message_t>
void MessageRecorder::RegisterChannel(const MessageRecorder::channel_subid_t subid, const std::string& alias) {
    auto* record_file = CreateFile(GetTypeName<message_t>(), subid, alias);

    RecordFileInitData new_record_data;
    new_record_data.message_type_name = GetTypeName<message_t>();
    new_record_data.subid             = subid;
    new_record_data.alias             = alias;
    record_init_map.emplace_back(new_record_data);

    this->Subscribe<message_t>(
        subid,
        [record_file](const std::shared_ptr<message_t>& message) { record_file->Write(MessageToStr(*message)); },
        subscription_strand_);
}

}  // namespace cris::core
