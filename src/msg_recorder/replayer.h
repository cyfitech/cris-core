#pragma once

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

    template<CRMessageType message_t>
    void RegisterChannel(const channel_subid_t subid);

    std::filesystem::path GetRecordDir() const;

    // Need to set before running
    void SetSpeedupRate(double rate);

    void MainLoop() override;

    void StopMainLoop() override;

   private:
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

    double                                   speed_up_rate_{1.0};
    cr_timestamp_nsec_t                      start_record_timestamp_{0};
    cr_timestamp_nsec_t                      start_local_timestamp_{0};
    std::atomic<bool>                        shutdown_flag_{false};
    std::filesystem::path                    record_dir_;
    std::vector<std::unique_ptr<RecordFile>> record_files_;
    RecordReaderPQueue                       record_readers_;
};

template<CRMessageType message_t>
void MessageReplayer::RegisterChannel(const MessageReplayer::channel_subid_t subid) {
    record_readers_.Push({
        .subid_            = subid,
        .msg_deserializer_ = [](const std::string& serialized_value) -> CRMessageBasePtr {
            auto msg = std::make_shared<message_t>();
            MessageFromStr(*msg, serialized_value);
            return msg;
        },
        .record_itr_ = GetRecordItr(GetTypeName<message_t>(), subid),
    });
}

}  // namespace cris::core
