#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

#include <execution>
#include <filesystem>

namespace cris::core {

MessageRecorder::MessageRecorder(const RecorderConfig& recorder_config, std::shared_ptr<JobRunner> runner)
    : Base(std::move(runner))
    , record_dir_(recorder_config.record_dir_ / RecordDirNameGenerator())
    , record_strand_(MakeStrand())
    , snapshot_config_intervals_(recorder_config.snapshot_intervals_)
    , snapshot_thread_(std::thread([this] { MessageRecorder::SnapshotWorker(); })) {
    std::filesystem::create_directories(record_dir_);
}

void MessageRecorder::SnapshotWorker() {
    if (snapshot_config_intervals_.empty()) {
        return;
    }

    if (snapshot_config_intervals_.size() > 1) {
        // TODO (YuzhouGuo, https://github.com/cyfitech/cris-core/issues/97) support multi-interval
        LOG(WARNING) << __func__
                     << ": More than one single interval received, multi-interval snapshot feature has not been "
                        "supported, only using the last interval specified";
    }

    const auto sleep_interval = snapshot_config_intervals_.back().interval_sec_;
    const auto kSkipThreshold = sleep_interval * 0.5;
    auto       wake_up_time   = std::chrono::steady_clock::now();

    while (!snapshot_shutdown_flag_.load()) {
        wake_up_time += sleep_interval;
        if ((wake_up_time - std::chrono::steady_clock::now()) > kSkipThreshold) {
            AddSnapshotJob();
        } else {
            LOG(WARNING) << __func__ << ": A snapshot job skipped due to job runner strand stuck.";
        }
        std::unique_lock<std::mutex> lck(snapshot_mtx_);
        snapshot_cv_.wait_until(lck, wake_up_time, [this] { return snapshot_shutdown_flag_.load(); });
    }
}

void MessageRecorder::AddSnapshotJob() {
    AddJobToRunner(
        [this]() {
            GenerateSnapshot();
            std::unique_lock<std::mutex> lck(snapshot_mtx_);
            snapshot_pause_flag_ = true;
            snapshot_cv_.notify_all();
        },
        record_strand_);
    std::unique_lock<std::mutex> lock(snapshot_mtx_);
    snapshot_cv_.wait(lock, [this] { return snapshot_pause_flag_; });
}

void MessageRecorder::SnapshotWorkerEnd() {
    if (auto runner = runner_weak_.lock()) {
        runner->Stop();
    }
    {
        std::unique_lock<std::mutex> lock(snapshot_mtx_);
        snapshot_shutdown_flag_.store(true);
    }
    snapshot_cv_.notify_all();
    if (snapshot_thread_.joinable()) {
        snapshot_thread_.join();
    }
    snapshot_path_map_.clear();
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
    {
        std::unique_lock<std::mutex> lock(snapshot_mtx_);
        std::for_each(std::execution::par_unseq, files_.begin(), files_.end(), [](auto& file) { file->CloseDB(); });
    }

    // create dir for individual interval
    const std::string     current_subdir_name = snapshot_config_intervals_.back().name_;
    std::filesystem::path interval_dir = record_dir_.parent_path() / std::string("snapshots") / current_subdir_name;
    const auto            snapshot_dir = interval_dir / SnapshotDirNameGenerator();
    const auto options = std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks;

    std::error_code ec;
    std::filesystem::create_directories(snapshot_dir, ec);
    if (ec.value() != 0) {
        LOG(ERROR) << __func__ << ": Failed to create snapshot directory " << snapshot_dir << ". " << ec.message();
    }

    std::filesystem::copy(record_dir_, snapshot_dir, options);

    if (!snapshot_path_map_.contains(current_subdir_name)) {
        snapshot_path_map_[current_subdir_name] = std::deque<std::filesystem::path>();
    }
    snapshot_path_map_[current_subdir_name].push_back(snapshot_dir);

    while (snapshot_path_map_[current_subdir_name].size() > snapshot_max_num_) {
        std::error_code ec;
        if (std::filesystem::remove_all(snapshot_path_map_[current_subdir_name].front(), ec) ==
            static_cast<std::uintmax_t>(-1)) {
            LOG(ERROR) << __func__ << ": Locally saved more than " << snapshot_max_num_
                       << " snapshot copies but failed to remove the oldest one. " << ec.message();
        }
        snapshot_path_map_[current_subdir_name].pop_front();
    }

    {
        std::unique_lock<std::mutex> lock(snapshot_mtx_);
        std::for_each(std::execution::par_unseq, files_.begin(), files_.end(), [](auto& file) { file->OpenDB(); });
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

const std::map<std::string, std::vector<std::filesystem::path>> MessageRecorder::GetSnapshotPaths() {
    std::map<std::string, std::vector<std::filesystem::path>> result_map;
    std::unique_lock<std::mutex>                              lock(snapshot_mtx_);

    // Paths in order of ascending directory construction time
    for (const auto& map_pair : snapshot_path_map_) {
        std::vector<std::filesystem::path> snapshot_reference_paths_(map_pair.second.begin(), map_pair.second.end());
        result_map[map_pair.first] = snapshot_reference_paths_;
    }
    return result_map;
}

RecordFile* MessageRecorder::CreateFile(
    const std::string&                     message_type,
    const MessageRecorder::channel_subid_t subid,
    const std::string&                     alias) {
    auto filename = impl::GetMessageRecordFileName(message_type, subid);
    auto path     = GetRecordDir() / filename;

    RecordFile* record_file;
    {
        std::unique_lock<std::mutex> lock(snapshot_mtx_);
        record_file = files_.emplace_back(std::make_unique<RecordFile>(path)).get();
    }

    if (!alias.empty()) {
        std::filesystem::create_symlink(filename, GetRecordDir() / alias);
    }

    return record_file;
}

}  // namespace cris::core
