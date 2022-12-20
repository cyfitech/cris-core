#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

#include <filesystem>
#include <queue>

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

    // Use a priority queue to record wake up times
    struct {
        bool operator()(const RecorderConfig::IntervalConfig l, const RecorderConfig::IntervalConfig r) const {
            return l.interval_sec_ > r.interval_sec_;
        }
    } compare;

    std::priority_queue wake_up_queue(snapshot_config_intervals_.begin(), snapshot_config_intervals_.end(), compare);

    auto origin_time  = std::chrono::steady_clock::now();
    auto wake_up_time = origin_time;

    while (!snapshot_shutdown_flag_.load()) {
        auto time = wake_up_queue.top();
        auto this_turn_sec_count =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - origin_time).count();

        // Record all the time intervals that satisfy the next snapshot condition
        while (this_turn_sec_count % time.interval_sec_.count() == 0) {
            snapshot_destination_intervals_.push_back(time);
            wake_up_queue.pop();
            if (wake_up_queue.empty()) {
                break;
            }
            time = wake_up_queue.top();
        }

        // Push the time intervals back for the next iteration
        for (const auto& current_time : snapshot_destination_intervals_) {
            wake_up_queue.push(current_time);
        }

        const auto kSkipThreshold = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        snapshot_destination_intervals_.front().interval_sec_) *
            0.5;
        wake_up_time += snapshot_destination_intervals_.front().interval_sec_;

        if ((wake_up_time - std::chrono::steady_clock::now()) > kSkipThreshold) {
            GenerateSnapshot();
        } else {
            LOG(WARNING) << __func__ << ": A snapshot job skipped: too close to the next snapshot timepoint.";
        }
        std::unique_lock lck(snapshot_mtx_);
        snapshot_cv_.wait_until(lck, wake_up_time, [this] { return snapshot_shutdown_flag_.load(); });
    }
}

void MessageRecorder::GenerateSnapshot() {
    bool snapshot_generated_flag = false;
    AddJobToRunner(
        [this, &snapshot_generated_flag]() {
            GenerateSnapshotImpl();
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

void MessageRecorder::GenerateSnapshotImpl() {
    std::lock_guard lck(snapshot_mtx_);

    std::for_each(files_.begin(), files_.end(), [](auto& file) { file->CloseDB(); });

    for (const auto& interval : snapshot_destination_intervals_) {
        // create dir for individual interval
        std::filesystem::path interval_dir = record_dir_.parent_path() / std::string("snapshots") / interval.name_;
        const auto            snapshot_dir = interval_dir / SnapshotDirNameGenerator();
        const auto options = std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks;

        if (std::error_code ec; !std::filesystem::create_directories(snapshot_dir, ec)) {
            LOG(ERROR) << __func__ << ": Failed to create snapshot directory " << snapshot_dir << ". " << ec.message();
        }

        {
            std::error_code ec;
            std::filesystem::copy(record_dir_, snapshot_dir, options, ec);
            if (ec) {
                LOG(ERROR) << __func__ << ": Failed to copy record directory " << record_dir_
                           << " to snapshot directory " << snapshot_dir << ". " << ec.message();
            }
        }

        auto& snapshot_dirs = snapshot_path_map_[interval.name_];
        snapshot_dirs.push_back(snapshot_dir);

        while (snapshot_dirs.size() > snapshot_max_num_) {
            if (std::error_code ec;
                std::filesystem::remove_all(snapshot_dirs.front(), ec) == static_cast<std::uintmax_t>(-1)) {
                LOG(ERROR) << __func__ << ": Locally saved more than " << snapshot_max_num_
                           << " snapshot copies but failed to remove the oldest one. " << ec.message();
            }
            snapshot_dirs.pop_front();
        }
    }

    std::for_each(files_.begin(), files_.end(), [](auto& file) { file->OpenDB(); });

    snapshot_destination_intervals_.clear();
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
