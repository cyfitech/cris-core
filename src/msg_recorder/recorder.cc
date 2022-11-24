#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

#include <filesystem>

namespace cris::core {

MessageRecorder::MessageRecorder(const RecorderConfigPtr recorder_ptr, std::shared_ptr<JobRunner> runner)
    : Base(std::move(runner))
    , record_dir_(std::filesystem::path(recorder_ptr->record_dir_) / RecordDirNameGenerator())
    , record_strand_(MakeStrand()) {
    std::filesystem::create_directories(record_dir_);
    if (recorder_ptr->enable_snapshot_) {
        SetSnapshotInterval(recorder_ptr->snapshot_intervals_);
    }
}

void MessageRecorder::SetSnapshotInterval(const std::vector<std::chrono::duration<int>>& intervals) {
    if (intervals.size() > 1) {
        // TODO (YuzhouGuo, https://github.com/cyfitech/cris-core/issues/97) support multi-intervals
        LOG(WARNING)
            << "More than one single interval received, multi-interval snapshot feature has not been supported";
        return;
    }
    if (snapshot_thread_.joinable()) {
        return;
    }
    snapshot_interval_    = intervals.back();
    snapshot_subdir_name_ = std::string("SECONDLY");
    snapshot_thread_      = std::thread(std::bind(&MessageRecorder::SnapshotWorkerStart, this));
}

void MessageRecorder::SnapshotWorkerStart() {
    while (!snapshot_shutdown_flag_.load()) {
        AddJobToRunner([this]() { GenerateSnapshot(keep_max_); }, record_strand_);
        std::unique_lock<std::mutex> lock(snapshot_mtx);
        snapshot_cv.wait_for(lock, snapshot_interval_, [this] { return snapshot_shutdown_flag_.load(); });
    }
}

void MessageRecorder::MakeSnapshot() {
    AddJobToRunner([this]() { GenerateSnapshot(keep_max_); }, record_strand_);
}

void MessageRecorder::SnapshotWorkerEnd() {
    {
        std::lock_guard<std::mutex> lock(snapshot_mtx);
        snapshot_shutdown_flag_.store(true);
        snapshot_cv.notify_all();
    }
    if (snapshot_thread_.joinable()) {
        snapshot_thread_.join();
    }
    snapshot_time_map_.clear();
}

MessageRecorder::~MessageRecorder() {
    SnapshotWorkerEnd();
    files_.clear();
    if (std::filesystem::is_empty(GetRecordDir())) {
        LOG(INFO) << "Record dir " << GetRecordDir() << " is empty, removing...";
        std::filesystem::remove(GetRecordDir());
    }
}

void MessageRecorder::GenerateSnapshot(const int& max) {
    for (const auto& file : files_) {
        file->CloseDB();
    }

    // create dir for individual interval
    std::filesystem::path interval_folder = record_dir_.parent_path() / snapshot_dir_name / snapshot_subdir_name_;
    std::filesystem::create_directories(interval_folder);
    std::filesystem::directory_entry withdraw;
    int                              counter = 0;
    for (auto const& dir_entry : std::filesystem::directory_iterator{interval_folder}) {
        counter++;
        if (!withdraw.exists() ||
            snapshot_time_map_[withdraw.path()].time_since_epoch() >
                snapshot_time_map_[dir_entry.path()].time_since_epoch()) {
            withdraw = dir_entry;
        }
    }

    // withdraw the earliest snapshot if needed
    if (counter >= max) {
        std::filesystem::remove_all(withdraw);
        snapshot_time_map_.erase(withdraw.path());
    }

    const auto snapshot_dir = interval_folder / SnapshotDirNameGenerator();
    std::filesystem::create_directories(snapshot_dir);
    std::filesystem::copy(record_dir_, snapshot_dir, std::filesystem::copy_options::recursive);
    snapshot_time_map_[snapshot_dir] = std::chrono::steady_clock::now();

    for (const auto& file : files_) {
        file->RestoreDB();
    }
}

std::string MessageRecorder::RecordDirNameGenerator() {
    return fmt::format("record.{:%Y%m%d-%H%M%S.%Z}.pid.{}", std::chrono::system_clock::now(), getpid());
}

std::string MessageRecorder::SnapshotDirNameGenerator() {
    return fmt::format("{:%Y%m%d-%H%M%S.%Z}", std::chrono::system_clock::now());
}

std::filesystem::path MessageRecorder::GetRecordDir() const {
    return record_dir_;
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
