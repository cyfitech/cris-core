#include "cris/core/msg_recorder/replayer.h"

#include "cris/core/utils/logging.h"

#include "impl/utils.h"

#include <thread>

namespace cris::core {

MessageReplayer::MessageReplayer(const std::filesystem::path& record_dir) : Base(), record_dir_(record_dir) {
}

std::filesystem::path MessageReplayer::GetRecordDir() const {
    return record_dir_;
}

void MessageReplayer::SetSpeedupRate(double rate) {
    speed_up_rate_ = rate;
}

void MessageReplayer::SetStartCallback(std::function<void()>&& on_start) {
    on_start_ = std::move(on_start);
}

void MessageReplayer::SetExitCallback(std::function<void()>&& on_exit) {
    on_exit_ = std::move(on_exit);
}

void MessageReplayer::MainLoop() {
    if (on_start_) {
        on_start_();
    }
    ReplayMessages();
    if (on_exit_) {
        on_exit_();
    }
}

bool MessageReplayer::ReplayMessages() {
    while (!shutdown_flag_.load() && !record_readers_.Empty()) {
        auto top_reader            = record_readers_.Pop();
        auto [key, serialized_val] = top_reader.record_itr_.Get();

        if (!start_record_timestamp_) {
            start_record_timestamp_ = key.timestamp_;
            start_local_timestamp_  = GetSystemTimestampNsec();
        } else {
            auto expect_elapse_time_ns =
                std::llround(static_cast<double>(key.timestamp_ - start_record_timestamp_) / speed_up_rate_);
            auto sleep_time = std::chrono::nanoseconds(
                expect_elapse_time_ns - static_cast<long long>(GetSystemTimestampNsec() - start_local_timestamp_));

            // If too close to the expect time, then not to sleep because of the scheduling overhead
            if (sleep_time > std::chrono::microseconds(10)) {
                std::this_thread::sleep_for(sleep_time);
            }
        }

        Publish(top_reader.subid_, top_reader.msg_deserializer_(serialized_val));

        top_reader.record_itr_.Next();
        if (top_reader.record_itr_.Valid()) {
            record_readers_.Push(std::move(top_reader));
        }
    }
    return record_readers_.Empty();
}

void MessageReplayer::StopMainLoop() {
    shutdown_flag_.store(true);
}

RecordFileIterator MessageReplayer::GetRecordItr(const std::string& message_type, channel_subid_t subid) {
    const auto file_path = GetRecordDir() / impl::GetMessageRecordFileName(message_type, subid);
    return record_files_.emplace_back(std::make_unique<RecordFile>(file_path))->Iterate();
}

void MessageReplayer::RecordReaderPQueue::Push(RecordReader reader) {
    record_readers_.push_back(std::move(reader));
    std::push_heap(record_readers_.begin(), record_readers_.end(), &Compare);
}

MessageReplayer::RecordReader MessageReplayer::RecordReaderPQueue::Pop() {
    std::pop_heap(record_readers_.begin(), record_readers_.end(), &Compare);
    auto back = std::move(record_readers_.back());
    record_readers_.pop_back();
    return back;
}

bool MessageReplayer::RecordReaderPQueue::Empty() const {
    return record_readers_.empty();
}

bool MessageReplayer::RecordReaderPQueue::Compare(const RecordReader& lhs, const RecordReader& rhs) {
    return RecordFileKey::compare(lhs.record_itr_.GetKey(), rhs.record_itr_.GetKey()) > 0;
}

}  // namespace cris::core
