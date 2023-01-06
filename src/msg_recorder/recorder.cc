#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace cris::core {

MessageRecorder::MessageRecorder(const RecorderConfig& recorder_config, std::shared_ptr<JobRunner> runner)
    : Base(std::move(runner))
    , record_dir_(recorder_config.record_dir_ / RecordDirNameGenerator())
    , record_strand_(MakeStrand())
    , snapshot_config_intervals_(recorder_config.snapshot_intervals_)
    , snapshot_thread_(std::thread([this] { SnapshotWorker(); })) {
    std::filesystem::create_directories(record_dir_);
}

void MessageRecorder::SnapshotWorker() {
    if (snapshot_config_intervals_.empty()) {
        return;
    }

    struct WakeUpConfig {
        RecorderConfig::IntervalConfig                     interval_config;
        std::chrono::time_point<std::chrono::steady_clock> wake_time;
    };

    std::vector<WakeUpConfig> snapshot_wakeup_intervals;
    for (const auto& interval : snapshot_config_intervals_) {
        snapshot_wakeup_intervals.push_back(WakeUpConfig{
            .interval_config = interval,
            .wake_time       = std::chrono::steady_clock::now(),
        });
    }

    std::priority_queue wake_up_queue(
        snapshot_wakeup_intervals.begin(),
        snapshot_wakeup_intervals.end(),
        [](const WakeUpConfig& lhs, const WakeUpConfig& rhs) { return lhs.wake_time >= rhs.wake_time; });

    auto current_snapshot_wakeup = wake_up_queue.top();
    wake_up_queue.pop();

    while (!snapshot_shutdown_flag_.load()) {
        const auto kSkipThreshold =
            std::chrono::duration_cast<std::chrono::milliseconds>(current_snapshot_wakeup.interval_config.period_) *
            0.5;

        if ((current_snapshot_wakeup.wake_time - std::chrono::steady_clock::now()) > kSkipThreshold) {
            GenerateSnapshot(current_snapshot_wakeup.interval_config);
        } else {
            LOG(WARNING) << __func__ << ": A snapshot job skipped: too close to the next snapshot timepoint.";
        }

        wake_up_queue.push(current_snapshot_wakeup);

        current_snapshot_wakeup = wake_up_queue.top();
        wake_up_queue.pop();

        std::unique_lock lck(snapshot_mtx_);
        snapshot_cv_.wait_until(lck, current_snapshot_wakeup.wake_time, [this] {
            return snapshot_shutdown_flag_.load();
        });
        current_snapshot_wakeup.wake_time += current_snapshot_wakeup.interval_config.period_;
    }
}

void MessageRecorder::GenerateSnapshot(const RecorderConfig::IntervalConfig& interval_config) {
    bool snapshot_generated_flag = false;
    AddJobToRunner(
        [this, &snapshot_generated_flag, &interval_config]() {
            GenerateSnapshotImpl(interval_config);
            {
                std::lock_guard lck(snapshot_mtx_);
                snapshot_generated_flag = true;
            }
            snapshot_cv_.notify_all();
        },
        record_strand_);
    std::unique_lock lock(snapshot_mtx_);
    snapshot_cv_.wait(lock, [&snapshot_generated_flag] { return snapshot_generated_flag; });
}

void MessageRecorder::StopSnapshotWorker() {
    if (auto runner = runner_weak_.lock()) {
        runner->Stop();
    }
    {
        std::lock_guard lck(snapshot_mtx_);
        snapshot_shutdown_flag_.store(true);
    }
    snapshot_cv_.notify_all();
    if (snapshot_thread_.joinable()) {
        snapshot_thread_.join();
    }
}

MessageRecorder::~MessageRecorder() {
    StopSnapshotWorker();
    files_.clear();
    if (std::filesystem::is_empty(GetRecordDir())) {
        LOG(INFO) << "Record dir " << GetRecordDir() << " is empty, removing...";
        std::filesystem::remove(GetRecordDir());
    }
}

void MessageRecorder::StopMainLoop() {
    StopSnapshotWorker();
}

void MessageRecorder::GenerateSnapshotImpl(const RecorderConfig::IntervalConfig& interval_config) {
    std::lock_guard lck(snapshot_mtx_);

    std::for_each(files_.begin(), files_.end(), [](auto& file) { file->CloseDB(); });

    // create dir for individual interval
    std::filesystem::path interval_dir = record_dir_.parent_path() / std::string("snapshots") / interval_config.name_;
    const auto            snapshot_dir = interval_dir / SnapshotDirNameGenerator();
    const auto options = std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks;

    if (std::error_code ec; !std::filesystem::create_directories(snapshot_dir, ec)) {
        LOG(ERROR) << __func__ << ": Failed to create snapshot directory " << snapshot_dir << ". " << ec.message();
    }

    {
        std::error_code ec;
        std::filesystem::copy(record_dir_, snapshot_dir, options, ec);
        if (ec) {
            LOG(ERROR) << __func__ << ": Failed to copy record directory " << record_dir_ << " to snapshot directory "
                       << snapshot_dir << ". " << ec.message();
        }
    }

    auto& snapshot_dirs = snapshot_path_map_[interval_config.name_];
    snapshot_dirs.push_back(snapshot_dir);

    while (snapshot_dirs.size() > snapshot_max_num_) {
        if (std::error_code ec;
            std::filesystem::remove_all(snapshot_dirs.front(), ec) == static_cast<std::uintmax_t>(-1)) {
            LOG(ERROR) << __func__ << ": Locally saved more than " << snapshot_max_num_
                       << " snapshot copies but failed to remove the oldest one. " << ec.message();
        }
        snapshot_dirs.pop_front();
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

std::map<std::string, std::vector<std::filesystem::path>> MessageRecorder::GetSnapshotPaths() {
    std::map<std::string, std::vector<std::filesystem::path>> result_map;
    std::lock_guard                                           lck(snapshot_mtx_);

    for (const auto& [interval_name, snapshot_path_list] : snapshot_path_map_) {
        result_map[interval_name] = std::vector(snapshot_path_list.begin(), snapshot_path_list.end());
    }
    return result_map;
}

RecordFile* MessageRecorder::CreateFile(
    const std::string&                     message_type,
    const MessageRecorder::channel_subid_t subid,
    const std::string&                     alias) {
    auto filename = impl::GetMessageRecordFileName(message_type, subid);
    auto path     = GetRecordDir() / filename;

    RecordFile* record_file = nullptr;
    {
        std::lock_guard lck(snapshot_mtx_);
        record_file = files_.emplace_back(std::make_unique<RecordFile>(path)).get();
    }

    if (!alias.empty()) {
        std::filesystem::create_symlink(filename, GetRecordDir() / alias);
    }

    return record_file;
}

}  // namespace cris::core
