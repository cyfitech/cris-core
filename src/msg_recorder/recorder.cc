#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

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
    snapshot_config_ = recorder_config.snapshot_intervals_.back();
    snapshot_thread_ = std::thread([this] { SnapshotWorkerStart(); });
}

void MessageRecorder::SnapshotWorkerStart() {
    static constexpr auto kEpselonMs   = std::chrono::milliseconds(100);
    auto                  origin_time  = std::chrono::system_clock::now();
    auto                  wake_up_time = origin_time;
    while (!snapshot_shutdown_flag_) {
        if (std::chrono::system_clock::now() <= (wake_up_time + kEpselonMs)) {
            MakeSnapshot();
        }
        wake_up_time += snapshot_config_.interval_sec_;
        std::unique_lock<std::mutex> lck(snapshot_mtx_);
        snapshot_cv_.wait_until(lck, wake_up_time, [this] { return snapshot_shutdown_flag_; });
    }
}

void MessageRecorder::MakeSnapshot() {
    std::unique_lock<std::mutex> lock(snapshot_mtx_);
    snapshot_pause_flag_ = false;
    AddJobToRunner(
        [this]() {
            GenerateSnapshot();
            snapshot_pause_flag_ = true;
            snapshot_cv_.notify_all();
        },
        record_strand_);

    // wait until the current generation to be finished
    snapshot_cv_.wait(lock, [this] { return snapshot_pause_flag_; });
}

void MessageRecorder::SnapshotWorkerEnd() {
    {
        std::unique_lock<std::mutex> lock(snapshot_mtx_);
        snapshot_shutdown_flag_ = true;
    }
    snapshot_cv_.notify_all();
    if (snapshot_thread_.joinable()) {
        snapshot_thread_.join();
    }
    snapshots_paths_.clear();
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

void MessageRecorder::GenerateSnapshot() {
    std::for_each(files_.begin(), files_.end(), [](auto& file) { file->CloseDB(); });

    // create dir for individual interval
    std::filesystem::path interval_dir = record_dir_.parent_path() / std::string("Snapshot") / snapshot_config_.name_;
    std::filesystem::create_directories(interval_dir);

    const auto snapshot_dir = interval_dir / SnapshotDirNameGenerator();
    const auto options      = std::filesystem::copy_options::recursive | std::filesystem::copy_options::skip_symlinks;
    std::filesystem::create_directories(snapshot_dir);
    std::filesystem::copy(record_dir_, snapshot_dir, options);

    snapshots_paths_.push_back(snapshot_dir);

    while (snapshots_paths_.size() > snapshot_max_num_) {
        std::error_code ec;
        if (std::filesystem::remove_all(snapshots_paths_.front(), ec) == static_cast<std::uintmax_t>(-1)) {
            LOG(ERROR) << __func__ << ": Locally saved more than " << snapshot_max_num_
                       << " snapshot copies but failed to remove the oldest one. " << ec.message();
        }
        snapshots_paths_.pop_front();
    }

    std::for_each(files_.begin(), files_.end(), [](auto& file) { file->OpenDB(); });
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
