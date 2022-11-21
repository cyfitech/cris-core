#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

#include <filesystem>

namespace cris::core {

MessageRecorder::MessageRecorder(const std::filesystem::path& record_dir_prefix, const int& snapshot_interval, std::shared_ptr<JobRunner> runner)
    : Base(std::move(runner))
    , record_dir_(record_dir_prefix / RecordDirNameGenerator())
    , record_strand_(runner ? runner->MakeStrand() : MakeStrand())
    , snapshot_interval_(snapshot_interval)
    , snapshot_thread_(std::bind(&MessageRecorder::SnapshotStart, this)) {
    std::filesystem::create_directories(record_dir_);
}

void MessageRecorder::SnapshotStart() {
    while (!snapshot_shutdown_flag_.load()) {
        AddJobToRunner([this]() { GenerateSnapshot(keep_max_); }, record_strand_);
        std::this_thread::sleep_for(std::chrono::seconds(snapshot_interval_));
    }
}

void MessageRecorder::SnapshotEnd() {
    snapshot_shutdown_flag_.store(true);
    if (snapshot_thread_.joinable()) {
        snapshot_thread_.join();
    }
    record_init_datas.clear();
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
    // remove symlinks and close DB
    for (const auto& file : files_) {
        if (!file->GetFilePath().empty() && !std::filesystem::is_empty(file->GetFilePath())) {
            std::filesystem::remove_all(file->GetFilePath());
        }
    }
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
    std::filesystem::copy(record_dir_, snapshot_dir, std::filesystem::copy_options::recursive | std::filesystem::copy_options::create_symlinks);

    // init DB back
    for (const auto& data : record_init_datas) {
        CreateFile(data.init_name, data.init_subid, data.init_alias);
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
