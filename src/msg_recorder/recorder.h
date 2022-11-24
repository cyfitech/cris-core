#pragma once

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

    explicit MessageRecorder(const std::filesystem::path& record_dir_prefix, std::shared_ptr<JobRunner> runner);

    MessageRecorder(const MessageRecorder&) = delete;
    MessageRecorder(MessageRecorder&&)      = delete;
    MessageRecorder& operator=(const MessageRecorder&) = delete;
    MessageRecorder& operator=(MessageRecorder&&) = delete;

    ~MessageRecorder();

    void SetSnapshotInterval(const std::vector<int64_t>& sec_intervals);

    void MakeInstantSnapshot();

    template<CRMessageType message_t>
    void RegisterChannel(const channel_subid_t subid, const std::string& alias = "");

    std::filesystem::path GetRecordDir() const;

   private:
    using msg_serializer = std::function<std::string(const CRMessageBasePtr&)>;

    unsigned long CreateFile(const std::string& message_type, const channel_subid_t subid, const std::string& alias);

    void GenerateSnapshot(const int& max);

    static std::string RecordDirNameGenerator();
    static std::string SnapshotDirNameGenerator();

    void SnapshotStart();
    void SnapshotEnd();

    void SetChronoDuration(const std::vector<int64_t>& sec_intervals);

    const std::filesystem::path                                            record_dir_;
    std::vector<std::unique_ptr<RecordFile>>                               files_;
    std::map<std::filesystem::path, std::chrono::steady_clock::time_point> snapshot_time_map_;
    std::shared_ptr<cris::core::JobRunnerStrand>                           record_strand_;

    struct RecordFileInitData {
        channel_subid_t init_subid;
        std::string     init_name;
        std::string     init_alias;
        std::string     init_path;
    };

    const int                       keep_max_{48};
    std::chrono::duration<int>      snapshot_interval_;
    std::vector<RecordFileInitData> record_init_data_;
    std::thread                     snapshot_thread_;
    std::atomic<bool>               snapshot_shutdown_flag_{false};
    std::mutex                      snapshot_mtx;
    std::condition_variable         snapshot_cv;
    std::string                     snapshot_subdir_name_;
    std::string                     snapshot_dir_name = std::string("Snapshot");
};

template<CRMessageType message_t>
void MessageRecorder::RegisterChannel(const MessageRecorder::channel_subid_t subid, const std::string& alias) {
    const std::string message_type = GetTypeName<message_t>();
    unsigned long     index        = CreateFile(message_type, subid, alias);

    this->Subscribe<message_t>(
        subid,
        [this, index](const std::shared_ptr<message_t>& message) {
            if (files_[index]) {
                files_[index]->Write(MessageToStr(*message));
            }
        },
        record_strand_);
}

}  // namespace cris::core
