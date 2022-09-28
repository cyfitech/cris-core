#include "cris/core/msg_recorder/record_file.h"
#include "cris/core/utils/logging.h"

#include <gflags/gflags.h>

#include <filesystem>

DEFINE_string(record_path, "", "Path to the old LevelDB record.");

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    auto old_file_path = std::filesystem::weakly_canonical(FLAGS_record_path);
    auto new_file_path = old_file_path.parent_path() / (old_file_path.filename().string() + ".converted.ldb.d");

    CHECK(!std::filesystem::exists(new_file_path)) << new_file_path << " exists.";

    cris::core::RecordFile old_file(old_file_path);
    cris::core::RecordFile new_file(new_file_path);

    for (auto itr = old_file.Iterate(); itr.Valid(); itr.Next()) {
        auto [key, value] = itr.Get();
        new_file.Write(key, value);
    }

    return 0;
}
