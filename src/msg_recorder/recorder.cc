#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

#include <filesystem>

namespace cris::core {

MessageRecorder::MessageRecorder(const std::filesystem::path& record_dir_prefix, std::shared_ptr<JobRunner> runner)
    : Base(std::move(runner))
    , record_dir_(record_dir_prefix / RecordDirNameGenerator())
    , record_strand_(MakeStrand()) {
    std::filesystem::create_directories(record_dir_);
}

void MessageRecorder::SnapshotStart() {
    while (!snapshot_shutdown_flag_.load()) {
        AddJobToRunner([this]() { GenerateSnapshot(keep_max_); }, record_strand_);
        std::unique_lock<std::mutex> lock(snapshot_mtx);
        snapshot_cv.wait_for(lock, std::chrono::seconds(snapshot_interval_), [this] {
            return snapshot_shutdown_flag_.load();
        });
    }
}

void MessageRecorder::SnapshotEnd() {
    {
        std::lock_guard<std::mutex> lock(snapshot_mtx);
        snapshot_shutdown_flag_.store(true);
        snapshot_cv.notify_all();
    }
    if (snapshot_thread_.joinable()) {
        snapshot_thread_.join();
    }
    record_init_datas_.clear();
}

MessageRecorder::~MessageRecorder() {
    SnapshotEnd();
    files_.clear();
    if (std::filesystem::is_empty(GetRecordDir())) {
        LOG(INFO) << "Record dir " << GetRecordDir() << " is empty, removing...";
        std::filesystem::remove(GetRecordDir());
    }
}

void MessageRecorder::GenerateSnapshot(const int& max) {
    // close DB
    files_.clear();

    // create dir for individual interval
    std::filesystem::path interval_folder = record_dir_ / std::string("Snapshot") / std::string("HOURLY");
    if (!std::filesystem::exists(interval_folder)) {
        std::filesystem::create_directories(interval_folder);
    }

    std::filesystem::directory_entry withdraw;
    int                              counter = 0;
    for (auto const& dir_entry : std::filesystem::directory_iterator{interval_folder}) {
        counter++;
        if (!withdraw.exists() ||
            (withdraw.last_write_time().time_since_epoch().count() -
             dir_entry.last_write_time().time_since_epoch().count()) > 0) {
            withdraw = dir_entry;
        }
    }

    // withdraw the earliest snapshot if needed
    if (counter >= max) {
        std::filesystem::remove_all(withdraw);
    }

    const auto snapshot_dir = interval_folder / SnapshotDirNameGenerator();
    std::filesystem::create_directories(snapshot_dir);
    std::filesystem::copy(record_dir_, snapshot_dir, std::filesystem::copy_options::recursive);

    // init DB back
    for (const auto& data : record_init_datas_) {
        files_.push_back(std::make_unique<RecordFile>(data.init_path));
    }
}

std::string MessageRecorder::RecordDirNameGenerator() {
    return fmt::format("record.{:%Y%m%d-%H%M%S.%Z}.pid.{}", std::chrono::system_clock::now(), getpid());
}

std::string MessageRecorder::SnapshotDirNameGenerator() {
    return fmt::format("{:%Y%m%d-%H%M%S.%Z}", std::chrono::system_clock::now());
}

void MessageRecorder::SetSnapshotInterval(const int& interval) {
    snapshot_interval_ = interval;
    snapshot_thread_   = std::thread(std::bind(&MessageRecorder::SnapshotStart, this));
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

    RecordFileInitData new_record_data;
    new_record_data.init_name  = message_type;
    new_record_data.init_subid = subid;
    new_record_data.init_alias = alias;
    new_record_data.init_path  = path;
    record_init_datas_.push_back(new_record_data);

    if (!alias.empty()) {
        std::filesystem::create_symlink(filename, GetRecordDir() / alias);
    }

    return record_file;
}

}  // namespace cris::core
