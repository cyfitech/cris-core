#pragma once

#include "cris/core/msg/message.h"
#include "cris/core/msg/node.h"
#include "cris/core/msg_recorder/record_file.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace cris::core {

class MessageRecorder : public CRNamedNode<MessageRecorder> {
   public:
    using Base = CRNamedNode<MessageRecorder>;

    explicit MessageRecorder(
        const std::filesystem::path& record_dir_prefix,
        const int&                   snapshot_interval,
        std::shared_ptr<JobRunner>   runner);

    MessageRecorder(const MessageRecorder&) = delete;
    MessageRecorder(MessageRecorder&&)      = delete;
    MessageRecorder& operator=(const MessageRecorder&) = delete;
    MessageRecorder& operator=(MessageRecorder&&) = delete;

    ~MessageRecorder();

    template<CRMessageType message_t>
    void RegisterChannel(const channel_subid_t subid, const std::string& alias = "");

    std::filesystem::path GetRecordDir() const;

   private:
    using msg_serializer = std::function<std::string(const CRMessageBasePtr&)>;

    RecordFile* CreateFile(const std::string& message_type, const channel_subid_t subid, const std::string& alias);

    void GenerateSnapshot(const int& max);

    static std::string RecordDirNameGenerator();
    static std::string SnapshotDirNameGenerator();

    void SnapshotStart();
    void SnapshotEnd();

    const std::filesystem::path              record_dir_;
    std::vector<std::unique_ptr<RecordFile>> files_;

    std::shared_ptr<cris::core::JobRunnerStrand> record_strand_;

    struct RecordFileInitData {
        channel_subid_t init_subid;
        std::string     init_name;
        std::string     init_alias;
    };

    const int                       keep_max_{48};
    int                             snapshot_interval_;
    std::vector<RecordFileInitData> record_init_datas;
    std::thread                     snapshot_thread_;
    std::atomic<bool>               snapshot_shutdown_flag_{false};
};

template<CRMessageType message_t>
void MessageRecorder::RegisterChannel(const MessageRecorder::channel_subid_t subid, const std::string& alias) {
    auto* record_file = CreateFile(GetTypeName<message_t>(), subid, alias);

    RecordFileInitData new_record_data;
    new_record_data.init_name  = GetTypeName<message_t>();
    new_record_data.init_subid = subid;
    new_record_data.init_alias = alias;
    record_init_datas.emplace_back(new_record_data);

    this->Subscribe<message_t>(
        subid,
        [record_file](const std::shared_ptr<message_t>& message) { record_file->Write(MessageToStr(*message)); },
        record_strand_);
}

}  // namespace cris::core
