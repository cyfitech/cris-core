#pragma once

#include "cris/core/msg/message.h"
#include "cris/core/msg/node.h"
#include "cris/core/msg_recorder/record_file.h"
#include "cris/core/msg_recorder/recorder_config.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace cris::core {

struct SnapshotInfo {
    std::filesystem::path snapshot_dir_;
    bool                  success{false};
};

class MessageRecorder : public CRNamedNode<MessageRecorder> {
   public:
    using Base = CRNamedNode<MessageRecorder>;

    explicit MessageRecorder(RecorderConfig recorder_config, std::shared_ptr<JobRunner> runner);

    MessageRecorder(const MessageRecorder&)            = delete;
    MessageRecorder(MessageRecorder&&)                 = delete;
    MessageRecorder& operator=(const MessageRecorder&) = delete;
    MessageRecorder& operator=(MessageRecorder&&)      = delete;

    ~MessageRecorder() override;

    void StopMainLoop() override;

    template<CRMessageType message_t>
    void RegisterChannel(const channel_subid_t subid, const std::string& alias = "");

    bool GenerateSnapshot(
        const std::filesystem::path&                        snapshot_dir,
        const std::optional<RecorderConfig::IntervalConfig> interval_config = std::nullopt);

    const std::filesystem::path& GetRecordDir() const;

    // Mapping from interval names to snapshot lists. Snapshots are ordered from old to new in the lists.
    std::map<std::string, std::vector<std::filesystem::path>> GetSnapshotPaths();

    void SetSnapshotJobPreStartCallback(
        std::function<void(const SnapshotInfo&, const std::optional<RecorderConfig::IntervalConfig>)>&& callback);

    void SetSnapshotJobPostFinishCallback(
        std::function<void(const SnapshotInfo&, const std::optional<RecorderConfig::IntervalConfig>)>&& callback);

   private:
    using msg_serializer = std::function<std::string(const CRMessageBasePtr&)>;

    RecordFile* CreateFile(const std::string& message_type, const channel_subid_t subid, const std::string& alias);

    bool GenerateSnapshotImpl(const std::filesystem::path& snapshot_dir);
    void MaintainMaxNumOfSnapshots(const RecorderConfig::IntervalConfig& interval_config);

    void SnapshotWorker();
    void StopSnapshotWorker();

    static std::string SnapshotDirNameGenerator();

    const RecorderConfig                         recorder_config_;
    std::vector<std::unique_ptr<RecordFile>>     files_;
    std::shared_ptr<cris::core::JobRunnerStrand> record_strand_;

    const std::size_t                                        snapshot_max_num_{48};
    std::atomic<bool>                                        snapshot_shutdown_flag_{false};
    std::mutex                                               snapshot_mtx_;
    std::condition_variable                                  snapshot_cv_;
    std::map<std::string, std::deque<std::filesystem::path>> snapshot_path_map_;
    std::thread                                              snapshot_thread_;

    std::function<void(const SnapshotInfo&, const std::optional<RecorderConfig::IntervalConfig>)> pre_start_;
    std::function<void(const SnapshotInfo&, const std::optional<RecorderConfig::IntervalConfig>)> post_finish_;
};

template<CRMessageType message_t>
void MessageRecorder::RegisterChannel(const MessageRecorder::channel_subid_t subid, const std::string& alias) {
    auto* record_file = CreateFile(GetTypeName<message_t>(), subid, alias);
    this->Subscribe<message_t>(
        subid,
        [record_file](const std::shared_ptr<message_t>& message) { record_file->Write(MessageToStr(*message)); },
        record_strand_);
}

std::unique_ptr<RollingHelper> CreateRollingHelper(const RecorderConfig::Rolling rolling);

}  // namespace cris::core
