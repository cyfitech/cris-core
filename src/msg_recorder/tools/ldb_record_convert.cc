#include "cris/core/msg_recorder/record_file.h"
#include "cris/core/utils/logging.h"

#include <gflags/gflags.h>

DEFINE_string(record_path, "", "Path to the old LevelDB record.");

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    cris::core::RecordFile old_file(FLAGS_record_path);
    cris::core::RecordFile new_file(FLAGS_record_path + ".new.ldb");

    for (auto itr = old_file.Iterate(); itr.Valid(); itr.Next()) {
        auto [key, value] = itr.Get();
        new_file.Write(key, value);
    }

    return 0;
}
