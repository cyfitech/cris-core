#include "cris/core/signal/cr_signal.h"

#include "gflags/gflags.h"

#include "gtest/gtest.h"

#include <cstdio>

int main(int argc, char** argv) {
    std::printf("Running main() from %s\n", __FILE__);
    ::testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, false);
    cris::core::InstallSignalHandler();
    return RUN_ALL_TESTS();
}
