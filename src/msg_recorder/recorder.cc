#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

#include <filesystem>

namespace cris::core {

MessageRecorder::MessageRecorder(const RecorderConfig& recorder_config, std::shared_ptr<JobRunner> runner)
    : Base(std::move(runner))
    , record_dir_(recorder_config.record_dir_ / RecordDirNameGenerator())
    , record_strand_(MakeStrand()) {
    std::filesystem::create_directories(record_dir_);
    SetSnapshotInterval(recorder_config);
}

void MessageRecorder::SetSnapshotInterval(const RecorderConfig& recorder_config) {
    if (recorder_config.snapshot_intervals_.empty()) {
        return;
    }
    if (recorder_config.snapshot_intervals_.size() > 1) {
        // TODO (YuzhouGuo, https://github.com/cyfitech/cris-core/issues/97) support multi-interval
        LOG(WARNING) << __func__
                     << "More than one single interval received, multi-interval snapshot feature has not been "
                        "supported, only using the last interval specified";
    }
    snapshot_interval_    = recorder_config.snapshot_intervals_.back().interval_sec_;
    snapshot_subdir_name_ = recorder_config.snapshot_intervals_.back().name_;
    snapshot_thread_      = std::thread(std::bind(&MessageRecorder::SnapshotWorkerStart, this));
}

void MessageRecorder::SnapshotWorkerStart() {
    while (!snapshot_shutdown_flag_.load()) {
        auto before = std::chrono::system_clock::now();
        MakeSnapshot();
        std::unique_lock<std::mutex> lck(snapshot_mtx_);
        snapshot_cv_.wait_until(lck, before + snapshot_interval_, [this] { return snapshot_shutdown_flag_.load(); });
    }
}

void MessageRecorder::MakeSnapshot() {
    std::unique_lock<std::mutex> lock(snapshot_mtx_);
    snapshot_pause_flag_.store(false);
    AddJobToRunner(
        [this]() {
            GenerateSnapshot(keep_max_);
            snapshot_pause_flag_.store(true);
            snapshot_cv_.notify_all();
        },
        record_strand_);

    // wait until the current generation to be finished
    snapshot_cv_.wait(lock, [this] { return snapshot_pause_flag_.load(); });
}

void MessageRecorder::SnapshotWorkerEnd() {
    {
        std::unique_lock<std::mutex> lock(snapshot_mtx_);
        snapshot_shutdown_flag_.store(true);
    }
    snapshot_cv_.notify_all();
    if (snapshot_thread_.joinable()) {
        snapshot_thread_.join();
    }
    snapshots_.clear();
}

MessageRecorder::~MessageRecorder() {
    SnapshotWorkerEnd();
    files_.clear();
    if (std::filesystem::is_empty(GetRecordDir())) {
        LOG(INFO) << "Record dir " << GetRecordDir() << " is empty, removing...";
        std::filesystem::remove(GetRecordDir());
    }
}

void MessageRecorder::StopMainLoop() {
    SnapshotWorkerEnd();
}

void MessageRecorder::GenerateSnapshot(const std::size_t max) {
    for (const auto& file : files_) {
        file->CloseDB();
    }

    // create dir for individual interval
    std::filesystem::path interval_dir = record_dir_.parent_path() / snapshot_dir_name_ / snapshot_subdir_name_;
    std::filesystem::create_directories(interval_dir);

    const auto snapshot_dir = interval_dir / SnapshotDirNameGenerator();
    const auto options      = std::filesystem::copy_options::recursive | std::filesystem::copy_options::skip_symlinks;
    std::filesystem::create_directories(snapshot_dir);
    std::filesystem::copy(record_dir_, snapshot_dir, options);

    snapshots_.push_back(snapshot_dir);

    while (snapshots_.size() > max) {
        auto old_snapshot = snapshots_.front();
        if (std::filesystem::exists(old_snapshot)) {
            std::uintmax_t n = std::filesystem::remove_all(old_snapshot);
            if (n > 0) {
                LOG(INFO) << "Removed " << n << " files or directories";
            } else {
                LOG(ERROR) << "Latest snapshot path fail to remove, something's wrong";
            }
        } else {
            LOG(ERROR) << "Latest snapshot path didn't exist, thus fail to remove";
        }
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
