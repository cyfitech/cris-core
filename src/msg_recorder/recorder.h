#pragma once

#include "cris/core/msg/message.h"
#include "cris/core/msg/node.h"
#include "cris/core/msg_recorder/record_file.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace cris::core {

class MessageRecorder : public CRNamedNode<MessageRecorder> {
   public:
    using Base = CRNamedNode<MessageRecorder>;

    explicit MessageRecorder(const std::filesystem::path& record_dir_prefix, std::shared_ptr<JobRunner> runner);

    MessageRecorder(const MessageRecorder&) = delete;

    MessageRecorder(MessageRecorder&&) = delete;

    MessageRecorder& operator=(const MessageRecorder&) = delete;

    ~MessageRecorder();

    template<CRMessageType message_t>
    void RegisterChannel(const channel_subid_t subid, const std::string& alias = "");

    std::filesystem::path GetRecordDir() const;

   private:
    using msg_serializer = std::function<std::string(const CRMessageBasePtr&)>;

    RecordFile* CreateFile(const std::string& message_type, const channel_subid_t subid, const std::string& alias);

    static std::string RecordDirNameGenerator();

    std::filesystem::path                    record_dir_;
    std::vector<std::unique_ptr<RecordFile>> files_;
};

template<CRMessageType message_t>
void MessageRecorder::RegisterChannel(const MessageRecorder::channel_subid_t subid, const std::string& alias) {
    auto file = CreateFile(GetTypeName<message_t>(), subid, alias);

    this->Subscribe<message_t>(
        subid,
        [file](const std::shared_ptr<message_t>& message) { file->Write(MessageToStr(*message)); },
        /* allow_concurrency = */ false);
}

}  // namespace cris::core
