#pragma once

#include "cris/core/config/recorder_config.h"
#include "cris/core/msg/message.h"
#include "cris/core/msg/node.h"
#include "cris/core/msg_recorder/record_file.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace cris::core {

class MessageRecorder : public CRNamedNode<MessageRecorder> {
   public:
    using Base = CRNamedNode<MessageRecorder>;

    explicit MessageRecorder(const RecorderConfigPtr recorder_config_ptr, std::shared_ptr<JobRunner> runner);

    MessageRecorder(const MessageRecorder&) = delete;
    MessageRecorder(MessageRecorder&&)      = delete;
    MessageRecorder& operator=(const MessageRecorder&) = delete;
    MessageRecorder& operator=(MessageRecorder&&) = delete;

    ~MessageRecorder();

    void StopMainLoop() override;

    template<CRMessageType message_t>
    void RegisterChannel(const channel_subid_t subid, const std::string& alias = "");

    void MakeSnapshot();

    std::filesystem::path GetRecordDir() const;

   private:
    using msg_serializer = std::function<std::string(const CRMessageBasePtr&)>;

    RecordFile* CreateFile(const std::string& message_type, const channel_subid_t subid, const std::string& alias);

    void SetSnapshotInterval(const RecorderConfigPtr recorder_config_ptr);
    void GenerateSnapshot(const std::size_t max);

    void SnapshotWorkerStart();
    void SnapshotWorkerEnd();

    static std::string RecordDirNameGenerator();
    static std::string SnapshotDirNameGenerator();

    const std::filesystem::path                  record_dir_;
    std::vector<std::unique_ptr<RecordFile>>     files_;
    std::shared_ptr<cris::core::JobRunnerStrand> record_strand_;

    const std::size_t                 keep_max_{48};
    std::chrono::seconds              snapshot_interval_;
    std::thread                       snapshot_thread_;
    std::atomic<bool>                 snapshot_shutdown_flag_{false};
    std::atomic<bool>                 snapshot_pause_flag_{false};
    std::mutex                        snapshot_mtx;
    std::condition_variable           snapshot_cv;
    std::string                       snapshot_subdir_name_;
    const std::string                 snapshot_dir_name_ = std::string("Snapshot");
    std::deque<std::filesystem::path> snapshots_;
};

template<CRMessageType message_t>
void MessageRecorder::RegisterChannel(const MessageRecorder::channel_subid_t subid, const std::string& alias) {
    auto* record_file = CreateFile(GetTypeName<message_t>(), subid, alias);
    this->Subscribe<message_t>(
        subid,
        [record_file](const std::shared_ptr<message_t>& message) { record_file->Write(MessageToStr(*message)); },
        record_strand_);
}

}  // namespace cris::core
