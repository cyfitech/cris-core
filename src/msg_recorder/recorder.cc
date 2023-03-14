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

MessageRecorder::MessageRecorder(RecorderConfig recorder_config, std::shared_ptr<JobRunner> runner)
    : Base(std::move(runner))
    , recorder_config_(std::move(recorder_config))
    , record_strand_(MakeStrand())
    , snapshot_thread_(std::thread([this] { SnapshotWorker(); })) {
}

MessageRecorder::MessageRecorder(
    RecorderConfig             recorder_config,
    std::shared_ptr<JobRunner> runner,
    FullRecordDirMaker&&       dir_maker)
    : Base(std::move(runner))
    , recorder_config_(std::move(recorder_config))
    , full_record_dir_maker_(std::move(dir_maker))
    , record_strand_(MakeStrand())
    , snapshot_thread_(std::thread([this] { SnapshotWorker(); })) {
}

void MessageRecorder::SetSnapshotJobPreStartCallback(
    std::function<void(const SnapshotInfo&, const std::optional<RecorderConfig::IntervalConfig>)>&& callback) {
    pre_start_ = std::move(callback);
}

void MessageRecorder::SetSnapshotJobPostFinishCallback(
    std::function<void(const SnapshotInfo&, const std::optional<RecorderConfig::IntervalConfig>)>&& callback) {
    post_finish_ = std::move(callback);
}

void MessageRecorder::SnapshotWorker() {
    const auto& snapshot_intervals = recorder_config_.snapshot_intervals_;
    if (snapshot_intervals.empty()) {
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
    for (const auto& interval : snapshot_intervals) {
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
            const auto snapshot_dir = GetRecordDir().parent_path() / std::string("snapshots") /
                current_snapshot_wakeup.interval_config.name_ / SnapshotDirNameGenerator();
            if (GenerateSnapshot(snapshot_dir, current_snapshot_wakeup.interval_config)) {
                std::lock_guard lck(snapshot_mtx_);
                snapshot_path_map_[current_snapshot_wakeup.interval_config.name_].push_back(snapshot_dir);
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
    const std::filesystem::path&                        snapshot_dir,
    const std::optional<RecorderConfig::IntervalConfig> interval_config) {
    bool         snapshot_generated_flag = false;
    SnapshotInfo info{
        .snapshot_dir_ = snapshot_dir,
    };
    AddJobToRunner(
        [this, &snapshot_generated_flag, &snapshot_dir, &interval_config, &info]() {
            if (pre_start_) {
                pre_start_(info, interval_config);
            }

            info.success = GenerateSnapshotImpl(snapshot_dir);
            {
                std::lock_guard lck(snapshot_mtx_);
                snapshot_generated_flag = true;
            }
            snapshot_cv_.notify_all();

            if (post_finish_) {
                post_finish_(info, interval_config);
            }
        },
        record_strand_);
    std::unique_lock lock(snapshot_mtx_);
    snapshot_cv_.wait(lock, [&snapshot_generated_flag] { return snapshot_generated_flag; });

    return info.success;
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

bool MessageRecorder::GenerateSnapshotImpl(const std::filesystem::path& snapshot_dir) {
    std::lock_guard lck(snapshot_mtx_);
    bool            generated_successful_flag = true;

    std::for_each(files_.begin(), files_.end(), [](auto& file) { file->CloseDB(); });

    const auto options = std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks;
    if (std::error_code ec; !std::filesystem::create_directories(snapshot_dir, ec)) {
        LOG(ERROR) << __func__ << ": Failed to create snapshot directory " << snapshot_dir << ". " << ec.message();
        generated_successful_flag = false;
    }

    {
        std::error_code ec;
        std::filesystem::copy(GetRecordDir(), snapshot_dir, options, ec);
        if (ec) {
            LOG(ERROR) << __func__ << ": Failed to copy record directory " << GetRecordDir()
                       << " to snapshot directory " << snapshot_dir << ". " << ec.message();
            generated_successful_flag = false;
        }
    }

    std::for_each(files_.begin(), files_.end(), [](auto& file) { file->OpenDB(); });

    return generated_successful_flag;
}

void MessageRecorder::MaintainMaxNumOfSnapshots(const RecorderConfig::IntervalConfig& interval_config) {
    auto& snapshot_dirs = snapshot_path_map_[interval_config.name_];
    while (snapshot_dirs.size() > snapshot_max_num_) {
        if (std::error_code ec;
            std::filesystem::remove_all(snapshot_dirs.front(), ec) == static_cast<std::uintmax_t>(-1)) {
            LOG(ERROR) << __func__ << ": Locally saved more than " << snapshot_max_num_
                       << " snapshot copies but failed to remove the oldest one. " << ec.message();
        }
        snapshot_dirs.pop_front();
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

const std::filesystem::path& MessageRecorder::GetRecordDir() const noexcept {
    return recorder_config_.record_dir_;
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
    namespace fs = std::filesystem;

    const auto filename = impl::GetMessageRecordFileName(message_type, subid);
    const auto dir      = full_record_dir_maker_ ? full_record_dir_maker_() : GetRecordDir() / RecordDirNameGenerator();

    {
        std::error_code ec{};
        if (!fs::create_directories(dir, ec) && ec) {
            LOG(ERROR) << __func__ << ": Failed to create directory " << dir << ", error \"" << ec.message() << "\".";
            return nullptr;
        }
    }

    const auto  path        = dir / filename;
    RecordFile* record_file = nullptr;
    {
        std::lock_guard lck(snapshot_mtx_);
        record_file = files_.emplace_back(std::make_unique<RecordFile>(path)).get();
    }

    if (!alias.empty()) {
        std::error_code ec{};
        fs::create_symlink(path, dir / alias, ec);
        LOG_IF(WARNING, ec) << __func__ << ": Failed to create symlink to " << dir << ", error \"" << ec.message()
                            << "\".";
    }

    return record_file;
}

}  // namespace cris::core
