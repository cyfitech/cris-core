#include "cris/core/msg_recorder/recorder.h"

#include "cris/core/utils/logging.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "impl/utils.h"

#include <filesystem>
#include <thread>

namespace cris::core {

void MessageRecorder::SnapshotTimer::SetSnapshotInterval(auto function, int interval) {
    this->clear = false;
    std::thread t([=]() {
        while (true) {
            if (this->clear) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::hours(interval));
            if (this->clear) {
                return;
            }
            function();
        }
    });
    t.detach();
}

void MessageRecorder::SnapshotTimer::SnapshotStop() {
    this->clear = true;
}

MessageRecorder::MessageRecorder(const std::filesystem::path& record_dir_prefix, std::shared_ptr<JobRunner> runner)
    : Base(std::move(runner))
    , record_dir_(record_dir_prefix / RecordDirNameGenerator())
    , subscription_strand_(runner ? runner->MakeStrand() : MakeStrand())
    , timer(SnapshotTimer()) {
    std::filesystem::create_directories(record_dir_);
    SnapshotStart();
}

void MessageRecorder::SnapshotStart() {
    for (const auto& [interval, max] : snapshot_records_) {
        const auto& alias_max = max;
        GenerateSnapshot(alias_max);
        timer.SetSnapshotInterval(
            [this, alias_max]() {
                AddJobToRunner([this, alias_max]() { GenerateSnapshot(alias_max); }, subscription_strand_);
            },
            int(interval));
    }
}

void MessageRecorder::SnapshotEnd() {
    timer.SnapshotStop();
    record_init_map.clear();
}

MessageRecorder::~MessageRecorder() {
    SnapshotEnd();
    files_.clear();

    if (std::filesystem::is_empty(GetRecordDir())) {
        LOG(INFO) << "Record dir " << GetRecordDir() << " is empty, removing...";
        std::filesystem::remove(GetRecordDir());
    }
}

void MessageRecorder::GenerateSnapshot(const MaxCopyNum& keep_max) {
    // close db and remove symlinks

    std::string prefix       = "";
    std::string keep_max_str = std::to_string(int(keep_max));
    switch (keep_max) {
        case MaxCopyNum::DAILY:
            prefix = "Daily_Snapshot_Max_" + keep_max_str;
            break;
        case MaxCopyNum::HOURLY:
            prefix = "Hourly_Snapshot_Max_" + keep_max_str;
            break;
        case MaxCopyNum::WEEKLY:
            prefix = "Weekly_Snapshot_Max_" + keep_max_str;
            break;
        default:
            break;
    }

    // create dir for individual interval
    std::filesystem::path interval_folder = record_dir_.parent_path() / prefix;
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
    if (counter >= int(keep_max)) {
        std::filesystem::remove_all(withdraw);
    }

    const auto snapshot_dir = interval_folder / SnapshotDirNameGenerator();
    std::filesystem::create_directories(snapshot_dir);
    std::filesystem::copy(record_dir_, snapshot_dir, std::filesystem::copy_options::recursive);

    // init db back
}

std::string MessageRecorder::RecordDirNameGenerator() {
    return fmt::format("record.{:%Y%m%d-%H%M%S.%Z}.pid.{}", std::chrono::system_clock::now(), getpid());
}

std::string MessageRecorder::SnapshotDirNameGenerator() {
    return fmt::format("snapshot.{:%Y%m%d-%H%M%S.%Z}.pid.{}", std::chrono::system_clock::now(), getpid());
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
