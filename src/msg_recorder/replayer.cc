#include "cris/core/msg_recorder/replayer.h"

#include "cris/core/utils/logging.h"

#include "impl/utils.h"

#include <stdexcept>
#include <thread>

namespace cris::core {

MessageReplayer::MessageReplayer(const std::filesystem::path& record_dir) : record_dir_(record_dir) {
}

std::filesystem::path MessageReplayer::GetRecordDir() const {
    return record_dir_;
}

void MessageReplayer::SetSpeedupRate(double rate) {
    speed_up_rate_ = rate;
}

void MessageReplayer::SetPostStartCallback(std::function<void()>&& callback) {
    post_start_ = std::move(callback);
}

void MessageReplayer::SetPreFinishCallback(std::function<void()>&& callback) {
    pre_finish_ = std::move(callback);
}

void MessageReplayer::SetPostFinishCallback(std::function<void()>&& callback) {
    post_finish_ = std::move(callback);
}

void MessageReplayer::MainLoop() {
    if (post_start_) {
        post_start_();
    }
    ReplayMessages();
    if (pre_finish_) {
        pre_finish_();
    }
    is_finished_.store(true);
    if (post_finish_) {
        post_finish_();
    }
}

void MessageReplayer::ReplayMessages() {
    while (!shutdown_flag_.load() && !record_readers_.Empty()) {
        auto top_reader            = record_readers_.Pop();
        auto [key, serialized_val] = top_reader.record_itr_.Get();

        if (!start_record_timestamp_) {
            start_record_timestamp_ = key.timestamp_ns_;
            start_local_timestamp_  = GetSystemTimestampNsec();
        } else {
            auto expect_elapse_time_ns =
                std::llround(static_cast<double>(key.timestamp_ns_ - start_record_timestamp_) / speed_up_rate_);
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
}

void MessageReplayer::StopMainLoop() {
    shutdown_flag_.store(true);
}

RecordFileIterator MessageReplayer::GetRecordItr(const std::string& message_type, channel_subid_t subid) {
    const auto dir = GetRecordDir() / impl::GetMessageFileName(message_type) / std::to_string(subid);

    const auto matched_files = impl::ListSubdirsWithSuffix(dir, impl::kLevelDbDirSuffix);
    if (matched_files.empty()) {
        LOG(ERROR) << "Record file with suffix \"" << impl::kLevelDbDirSuffix << "\" not found, record dir " << dir;
        throw std::logic_error{"No matched record file found."};
    }

    // TODO(yanglu1225): Support rolled (multiple) record dirs
    LOG_IF(WARNING, matched_files.size() > 1)
        << "Found multiple record files (dirs), is the record data rolled? Will use the first one (in lexical order)="
        << matched_files[0];

    const auto& file_path = matched_files[0];
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
