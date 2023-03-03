#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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

void MessageRecorder::SetSnapshotJobPreStartCallback(
    std::function<void(const std::optional<RecorderConfig::IntervalConfig>, const SnapshotInfo&)>&& callback) {
    pre_start_ = std::move(callback);
}

void MessageRecorder::SetSnapshotJobPostFinishCallback(
    std::function<void(const std::optional<RecorderConfig::IntervalConfig>, const SnapshotInfo&)>&& callback) {
    post_finish_ = std::move(callback);
}

void MessageRecorder::SnapshotWorker() {
    if (snapshot_config_intervals_.empty()) {
        return;
    }

    struct WakeUpConfig {
        RecorderConfig::IntervalConfig                     interval_config;
        std::chrono::time_point<std::chrono::steady_clock> wake_time;
    };

    auto compare = [](const WakeUpConfig& lhs, const WakeUpConfig& rhs) {
        if (lhs.wake_time != rhs.wake_time) {
            return lhs.wake_time > rhs.wake_time;
        }
        return lhs.interval_config.period_.count() > rhs.interval_config.period_.count();
    };

    std::priority_queue<WakeUpConfig, std::vector<WakeUpConfig>, decltype(compare)> wake_up_queue;

    const auto current_time = std::chrono::steady_clock::now();
    for (const auto& interval : snapshot_config_intervals_) {
        wake_up_queue.push(WakeUpConfig{
            .interval_config = interval,
            .wake_time       = current_time,
        });
    }

    auto current_snapshot_wakeup = wake_up_queue.top();
    wake_up_queue.pop();

    while (!snapshot_shutdown_flag_.load()) {
        const auto kSkipThreshold =
            std::chrono::duration_cast<std::chrono::milliseconds>(current_snapshot_wakeup.interval_config.period_) *
            0.5;

        const auto next_wake_time_for_this_interval =
            current_snapshot_wakeup.wake_time + current_snapshot_wakeup.interval_config.period_;

        if ((next_wake_time_for_this_interval - std::chrono::steady_clock::now()) > kSkipThreshold) {
            SnapshotInfo current_info;
            current_info.snapshot_dir_ = record_dir_.parent_path() / std::string("snapshots") /
                current_snapshot_wakeup.interval_config.name_ / SnapshotDirNameGenerator();
            if (GenerateSnapshot(current_snapshot_wakeup.interval_config, current_info)) {
                std::lock_guard lck(snapshot_mtx_);
                snapshot_info_map_[current_snapshot_wakeup.interval_config.name_].push_back(current_info);
                MaintainMaxNumOfSnapshots(current_snapshot_wakeup.interval_config);
            }
        } else {
            LOG(WARNING) << __func__ << ": A snapshot job skipped: too close to the next snapshot timepoint.";
        }

        current_snapshot_wakeup.wake_time = next_wake_time_for_this_interval;

        wake_up_queue.push(current_snapshot_wakeup);
        current_snapshot_wakeup = wake_up_queue.top();
        wake_up_queue.pop();

        std::unique_lock lock(snapshot_mtx_);
        snapshot_cv_.wait_until(lock, current_snapshot_wakeup.wake_time, [this] {
            return snapshot_shutdown_flag_.load();
        });
    }
}

bool MessageRecorder::GenerateSnapshot(
    const std::optional<RecorderConfig::IntervalConfig> interval_config,
    const SnapshotInfo&                                 snapshot_info) {
    bool successful_flag         = false;
    bool snapshot_generated_flag = false;
    AddJobToRunner(
        [this, &snapshot_generated_flag, &successful_flag, &snapshot_info, &interval_config]() {
            if (pre_start_) {
                pre_start_(interval_config, snapshot_info);
            }

            successful_flag = GenerateSnapshotImpl(snapshot_info);
            {
                std::lock_guard lck(snapshot_mtx_);
                snapshot_generated_flag = true;
            }
            snapshot_cv_.notify_all();

            if (post_finish_) {
                post_finish_(interval_config, snapshot_info);
            }
        },
        record_strand_);
    std::unique_lock lock(snapshot_mtx_);
    snapshot_cv_.wait(lock, [&snapshot_generated_flag] { return snapshot_generated_flag; });

    return successful_flag;
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

bool MessageRecorder::GenerateSnapshotImpl(const SnapshotInfo& snapshot_info) {
    std::lock_guard lck(snapshot_mtx_);
    bool            generated_successful_flag = true;

    std::for_each(files_.begin(), files_.end(), [](auto& file) { file->CloseDB(); });

    const auto options = std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks;
    if (std::error_code ec; !std::filesystem::create_directories(snapshot_info.snapshot_dir_, ec)) {
        LOG(ERROR) << __func__ << ": Failed to create snapshot directory " << snapshot_info.snapshot_dir_ << ". "
                   << ec.message();
        generated_successful_flag = false;
    }

    {
        std::error_code ec;
        std::filesystem::copy(record_dir_, snapshot_info.snapshot_dir_, options, ec);
        if (ec) {
            LOG(ERROR) << __func__ << ": Failed to copy record directory " << record_dir_ << " to snapshot directory "
                       << snapshot_info.snapshot_dir_ << ". " << ec.message();
            generated_successful_flag = false;
        }
    }

    std::for_each(files_.begin(), files_.end(), [](auto& file) { file->OpenDB(); });

    return generated_successful_flag;
}

void MessageRecorder::MaintainMaxNumOfSnapshots(const RecorderConfig::IntervalConfig& interval_config) {
    auto& snapshot_info_list = snapshot_info_map_[interval_config.name_];
    while (snapshot_info_list.size() > snapshot_max_num_) {
        if (std::error_code ec; std::filesystem::remove_all(snapshot_info_list.front().snapshot_dir_, ec) ==
            static_cast<std::uintmax_t>(-1)) {
            LOG(ERROR) << __func__ << ": Locally saved more than " << snapshot_max_num_
                       << " snapshot copies but failed to remove the oldest one. " << ec.message();
        }
        snapshot_info_list.pop_front();
    }
}

std::string MessageRecorder::RecordDirNameGenerator() {
    return fmt::format("record.{:%Y%m%d-%H%M%S.%Z}.pid.{}", std::chrono::system_clock::now(), getpid());
}

std::string MessageRecorder::SnapshotDirNameGenerator() {
    using print_duration_t            = std::chrono::nanoseconds;
    constexpr auto print_tick_per_sec = print_duration_t::period::den / print_duration_t::period::num;
    const auto     print_precision    = static_cast<int>(std::log10(print_tick_per_sec));

    auto now = std::chrono::system_clock::now();
    return fmt::format(
        "{:%Y%m%d-%H%M%S}.{:0{}}.{:%Z}",
        std::chrono::time_point_cast<std::chrono::seconds>(now),
        std::chrono::duration_cast<print_duration_t>(now.time_since_epoch()).count() % print_tick_per_sec,
        print_precision,
        now);
}

std::filesystem::path MessageRecorder::GetRecordDir() const {
    return record_dir_;
}

std::map<std::string, std::vector<SnapshotInfo>> MessageRecorder::GetSnapshotInfoMap() {
    std::map<std::string, std::vector<SnapshotInfo>> result_map;
    std::lock_guard                                  lck(snapshot_mtx_);

    for (const auto& [interval_name, snapshot_info_list] : snapshot_info_map_) {
        result_map[interval_name] = std::vector(snapshot_info_list.begin(), snapshot_info_list.end());
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
