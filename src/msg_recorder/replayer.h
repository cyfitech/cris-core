#pragma once

#include "cris/core/msg/message.h"
#include "cris/core/msg/node.h"
#include "cris/core/msg_recorder/record_file.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <queue>
#include <string>
#include <vector>

namespace cris::core {

class MessageReplayer : public CRNamedNode<MessageReplayer> {
   public:
    using Base = CRNamedNode<MessageReplayer>;

    explicit MessageReplayer(const std::filesystem::path& record_dir);

    MessageReplayer(const MessageReplayer&) = delete;

    MessageReplayer(MessageReplayer&&) = delete;

    MessageReplayer& operator=(const MessageReplayer&) = delete;

    MessageReplayer& operator=(MessageReplayer&&) = delete;

    ~MessageReplayer() override = default;

    template<CRMessageType message_t>
    bool RegisterChannel(const channel_subid_t subid);

    std::filesystem::path GetRecordDir() const;

    bool IsEnded() const { return is_finished_.load(); }

    // Need to set before running
    void SetSpeedupRate(double rate);

    void SetPostStartCallback(std::function<void()>&& callback);

    void SetPreFinishCallback(std::function<void()>&& callback);

    void SetPostFinishCallback(std::function<void()>&& callback);

    void MainLoop() override;

    void StopMainLoop() override;

   protected:
    RecordFileIterator GetRecordItr(const std::string& message_type, channel_subid_t subid);

    struct RecordReader {
        channel_subid_t                                     subid_{0};
        std::function<CRMessageBasePtr(const std::string&)> msg_deserializer_;
        RecordFileIterator                                  record_itr_;
    };

    class RecordReaderPQueue {
       public:
        void Push(RecordReader reader);

        RecordReader Pop();

        bool Empty() const;

        static bool Compare(const RecordReader& lhs, const RecordReader& rhs);

       private:
        std::vector<RecordReader> record_readers_;
    };

    virtual void ReplayMessages();

    double                                   speed_up_rate_{1.0};
    cr_timestamp_nsec_t                      start_record_timestamp_{0};
    cr_timestamp_nsec_t                      start_local_timestamp_{0};
    std::atomic<bool>                        shutdown_flag_{false};
    std::filesystem::path                    record_dir_;
    std::vector<std::unique_ptr<RecordFile>> record_files_;
    RecordReaderPQueue                       record_readers_;
    std::atomic<bool>                        is_finished_{false};

    std::function<void()> post_start_;
    std::function<void()> pre_finish_;
    std::function<void()> post_finish_;
};

template<CRMessageType message_t>
bool MessageReplayer::RegisterChannel(const MessageReplayer::channel_subid_t subid) {
    auto itr = GetRecordItr(GetTypeName<message_t>(), subid);
    if (!itr.Valid()) {
        return false;
    }
    record_readers_.Push({
        .subid_            = subid,
        .msg_deserializer_ = [](const std::string& serialized_value) -> CRMessageBasePtr {
            auto msg = std::make_shared<message_t>();
            MessageFromStr(*msg, serialized_value);
            return msg;
        },
        .record_itr_ = std::move(itr),
    });
    return true;
}

}  // namespace cris::core
