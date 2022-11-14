#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

#include <filesystem>

namespace cris::core {

MessageRecorder::MessageRecorder(const std::filesystem::path& record_dir_prefix, std::shared_ptr<JobRunner> runner)
    : Base(std::move(runner))
    , record_dir_(record_dir_prefix / RecordDirNameGenerator()) {
    auto temp_record_dir = record_dir_prefix;
    temp_record_dir += std::string("_snapshots");
    record_snapshot_dir_ = temp_record_dir;

    std::filesystem::create_directories(record_dir_);
    std::filesystem::create_directories(record_snapshot_dir_);

    Timer t = Timer();
    for (const auto& [type, pair] : snapshot_intervals_) {
        GenerateSnapshot(pair.second);
        const int& alias = pair.second;
        t.setInterval([this, alias]() { GenerateSnapshot(alias); }, pair.first);
    }
}

MessageRecorder::~MessageRecorder() {
    files_.clear();

    if (std::filesystem::is_empty(GetRecordDir())) {
        LOG(INFO) << "Record dir " << GetRecordDir() << " is empty, removing...";
        std::filesystem::remove(GetRecordDir());
    }

    if (std::filesystem::is_empty(GetRecordSnapshotDir())) {
        LOG(INFO) << "Record Snapshot dir " << GetRecordSnapshotDir() << " is empty, removing...";
        std::filesystem::remove(GetRecordSnapshotDir());
    }
}

void MessageRecorder::GenerateSnapshot(const int& keep_max) {
    data_copied_ = false;
    std::unique_lock lock(recorder_mtx_);
    snapshot_cv_.wait(lock, [this] { return dataflow_paused; });

    std::filesystem::directory_entry withdraw;
    int                              counter = 0;
    for (auto const& dir_entry : std::filesystem::directory_iterator{record_snapshot_dir_}) {
        counter++;
        if (!withdraw.exists() ||
            (withdraw.last_write_time().time_since_epoch().count() -
             dir_entry.last_write_time().time_since_epoch().count()) > 0) {
            withdraw = dir_entry;
        }
    }

    // withdraw the earliest snapshot if needed
    if (counter >= keep_max) {
        std::filesystem::remove_all(withdraw);
    }

    // generate the latest snapshot dir
    auto result_record_dir = record_snapshot_dir_;
    result_record_dir += std::string("/");
    result_record_dir += std::string(RecordDirNameGenerator());
    std::filesystem::create_directories(result_record_dir);
    std::filesystem::copy(record_dir_, result_record_dir, std::filesystem::copy_options::recursive);

    data_copied_ = true;
    snapshot_cv_.notify_all();
}

std::string MessageRecorder::RecordDirNameGenerator() {
    return fmt::format("record.{:%Y%m%d-%H%M%S.%Z}.pid.{}", std::chrono::system_clock::now(), getpid());
}

std::filesystem::path MessageRecorder::GetRecordDir() const {
    return record_dir_;
}

std::filesystem::path MessageRecorder::GetRecordSnapshotDir() const {
    return record_snapshot_dir_;
}

RecordFile* MessageRecorder::CreateFile(
    const std::string&                     message_type,
    const MessageRecorder::channel_subid_t subid,
    const std::string&                     alias) {
    auto filename = impl::GetMessageRecordFileName(message_type, subid);
    auto path     = GetRecordDir() / filename;

    auto* record_file = files_.emplace_back(std::make_unique<RecordFile>(path)).get();

    if (!alias.empty()) {
        std::filesystem::create_symlink(filename, GetRecordDir() / alias);
    }

    return record_file;
}

}  // namespace cris::core
