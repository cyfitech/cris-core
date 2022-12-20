#pragma once

#include "cris/core/config/recorder_config.h"
#include "cris/core/msg/message.h"
#include "cris/core/msg/node.h"
#include "cris/core/msg_recorder/record_file.h"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace cris::core {

class MessageRecorder : public CRNamedNode<MessageRecorder> {
   public:
    using Base = CRNamedNode<MessageRecorder>;

    explicit MessageRecorder(const RecorderConfig& recorder_config, std::shared_ptr<JobRunner> runner);

    MessageRecorder(const MessageRecorder&) = delete;
    MessageRecorder(MessageRecorder&&)      = delete;
    MessageRecorder& operator=(const MessageRecorder&) = delete;
    MessageRecorder& operator=(MessageRecorder&&) = delete;

    ~MessageRecorder();

    void StopMainLoop() override;

    template<CRMessageType message_t>
    void RegisterChannel(const channel_subid_t subid, const std::string& alias = "");

    void GenerateSnapshot();

    std::filesystem::path GetRecordDir() const;

    // Mapping from interval names to snapshot lists. Snapshots are ordered from old to new in the lists.
    std::map<std::string, std::vector<std::filesystem::path>> GetSnapshotPaths();

   private:
    using msg_serializer = std::function<std::string(const CRMessageBasePtr&)>;

    RecordFile* CreateFile(const std::string& message_type, const channel_subid_t subid, const std::string& alias);

    void GenerateSnapshotImpl();

    void SnapshotWorker();
    void StopSnapshotWorker();

    static std::string RecordDirNameGenerator();
    static std::string SnapshotDirNameGenerator();

    const std::filesystem::path                  record_dir_;
    std::vector<std::unique_ptr<RecordFile>>     files_;
    std::shared_ptr<cris::core::JobRunnerStrand> record_strand_;

    const std::size_t                                        snapshot_max_num_{48};
    const std::vector<RecorderConfig::IntervalConfig>        snapshot_config_intervals_;
    std::atomic<bool>                                        snapshot_shutdown_flag_{false};
    std::mutex                                               snapshot_mtx_;
    std::condition_variable                                  snapshot_cv_;
    std::map<std::string, std::deque<std::filesystem::path>> snapshot_path_map_;
    std::vector<RecorderConfig::IntervalConfig>              snapshot_destination_intervals_;
    std::thread                                              snapshot_thread_;
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
