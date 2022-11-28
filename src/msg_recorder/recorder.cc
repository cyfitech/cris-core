#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

#include <filesystem>

namespace cris::core {

MessageRecorder::MessageRecorder(const RecorderConfigPtr recorder_config_ptr, std::shared_ptr<JobRunner> runner)
    : Base(std::move(runner))
    , record_dir_(recorder_config_ptr->record_dir_ / RecordDirNameGenerator())
    , record_strand_(MakeStrand()) {
    std::filesystem::create_directories(record_dir_);
    SetSnapshotInterval(recorder_config_ptr);
}

void MessageRecorder::SetSnapshotInterval(const RecorderConfigPtr recorder_config_ptr) {
    if (recorder_config_ptr->snapshot_intervals_.empty()) {
        return;
    }
    if (recorder_config_ptr->snapshot_intervals_.size() > 1) {
        // TODO (YuzhouGuo, https://github.com/cyfitech/cris-core/issues/97) support multi-intervals
        LOG(WARNING) << "More than one single interval received, multi-interval snapshot feature has not been "
                        "supported, only using the last interval specified";
    }
    snapshot_interval_    = recorder_config_ptr->snapshot_intervals_.back();
    snapshot_subdir_name_ = recorder_config_ptr->interval_name_;
    snapshot_threads_.push_back(std::thread(std::bind(&MessageRecorder::SnapshotWorkerStart, this)));
}

void MessageRecorder::SnapshotWorkerStart() {
    while (!snapshot_shutdown_flag_.load()) {
        auto before = std::chrono::system_clock::now();
        MakeSnapshot();
        std::unique_lock<std::mutex> lock(snapshot_mtx);
        auto                         after      = std::chrono::system_clock::now();
        auto                         difference = std::chrono::duration_cast<std::chrono::seconds>(after - before);
        snapshot_cv.wait_until(lock, after + snapshot_interval_ - difference, [this] {
            return snapshot_shutdown_flag_.load();
        });
    }
}

void MessageRecorder::MakeSnapshot() {
    snapshot_threads_.push_back(std::thread(std::bind(&MessageRecorder::DoSnapshotWork, this)));
}

void MessageRecorder::DoSnapshotWork() {
    std::unique_lock<std::mutex> lock(snapshot_mtx);
    snapshot_pause_flag_.store(false);
    AddJobToRunner(
        [this]() {
            GenerateSnapshot(keep_max_);
            snapshot_pause_flag_.store(true);
        },
        record_strand_);

    // wait until the current generation to be finished
    snapshot_cv.wait(lock, [this] { return snapshot_pause_flag_.load(); });
}

void MessageRecorder::SnapshotWorkerEnd() {
    {
        std::lock_guard<std::mutex> lock(snapshot_mtx);
        snapshot_shutdown_flag_.store(true);
        snapshot_cv.notify_all();
    }
    for (auto& thread : snapshot_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    snapshots_.clear();
    snapshot_threads_.clear();
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
    std::filesystem::path interval_folder = record_dir_.parent_path() / snapshot_dir_name_ / snapshot_subdir_name_;
    std::filesystem::create_directories(interval_folder);

    const auto snapshot_dir = interval_folder / SnapshotDirNameGenerator();
    std::filesystem::create_directories(snapshot_dir);
    std::filesystem::copy(record_dir_, snapshot_dir, std::filesystem::copy_options::recursive);

    snapshots_.push_back(snapshot_dir);

    while (int(snapshots_.size()) > max) {
        auto old_snapshot = snapshots_.front();
        std::filesystem::remove_all(old_snapshot);
        snapshots_.pop_front();
    }

    for (const auto& file : files_) {
        file->OpenDB();
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
