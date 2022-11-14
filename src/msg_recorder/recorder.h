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
#include <thread>
#include <utility>

namespace cris::core {

class Timer {
    bool clear = false;

   public:
    template<typename Function>
    void setInterval(Function function, int interval);
};

void Timer::setInterval(auto function, int interval) {
    this->clear = false;
    std::thread t([=]() {
        while (true) {
            if (this->clear)
                return;
            std::this_thread::sleep_for(std::chrono::hours(interval));
            if (this->clear)
                return;
            function();
        }
    });
    t.detach();
}

class MessageRecorder : public CRNamedNode<MessageRecorder> {
   public:
    using Base = CRNamedNode<MessageRecorder>;

    explicit MessageRecorder(const std::filesystem::path& record_dir_prefix, std::shared_ptr<JobRunner> runner);

    MessageRecorder(const MessageRecorder&) = delete;
    // MessageRecorder(MessageRecorder&&)      = default;
    MessageRecorder& operator=(const MessageRecorder&) = delete;
    MessageRecorder& operator=(MessageRecorder&&) = delete;

    ~MessageRecorder();

    template<CRMessageType message_t>
    void RegisterChannel(const channel_subid_t subid, const std::string& alias = "");

    std::filesystem::path GetRecordDir() const;

    std::filesystem::path GetRecordSnapshotDir() const;

   private:
    using msg_serializer = std::function<std::string(const CRMessageBasePtr&)>;

    RecordFile* CreateFile(const std::string& message_type, const channel_subid_t subid, const std::string& alias);

    void GenerateSnapshot(const int& keep_max);

    static std::string RecordDirNameGenerator();

    const std::filesystem::path              record_dir_;
    std::filesystem::path                    record_snapshot_dir_;
    std::vector<std::unique_ptr<RecordFile>> files_;

    std::map<std::string, std::pair<int, int>> snapshot_intervals_{
        {"HOURLY", std::make_pair(1, 48)},
        {"DAILY", std::make_pair(24, 14)},
        {"WEEKLY", std::make_pair(168, 7)},
    };

    std::mutex              recorder_mtx_;
    std::condition_variable snapshot_cv_;
    bool                    dataflow_paused{false};
    bool                    data_copied_{false};
};

template<CRMessageType message_t>
void MessageRecorder::RegisterChannel(const MessageRecorder::channel_subid_t subid, const std::string& alias) {
    auto* record_file = CreateFile(GetTypeName<message_t>(), subid, alias);

    this->Subscribe<message_t>(
        subid,
        [this, record_file, alias](const std::shared_ptr<message_t>& message) {
            if (!data_copied_) {
                std::unique_lock lock(recorder_mtx_);
                dataflow_paused = true;
                snapshot_cv_.notify_all();
                snapshot_cv_.wait(lock, [this] { return data_copied_; });
            }
            record_file->Write(MessageToStr(*message));
            dataflow_paused = false;
        },
        /* allow_concurrency = */ false);
}

}  // namespace cris::core
