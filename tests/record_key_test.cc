#include "cris/core/msg_recorder/record_file.h"

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace cris::core {

#define EXPECT_KEY_EQ(lhs, rhs)                         \
    do {                                                \
        EXPECT_EQ((lhs).timestamp_, (rhs).timestamp_);  \
        EXPECT_EQ((lhs).count_, (rhs).count_);          \
        EXPECT_EQ(RecordFileKey::compare(lhs, rhs), 0); \
    } while (0)

TEST(RecordKeyTest, Convert) {
    {
        RecordFileKey key = {
            .timestamp_ = 1234567,
            .count_     = 0,
        };

        EXPECT_KEY_EQ(key, RecordFileKey::FromBytes(key.ToBytes()));
    }

    {
        RecordFileKey key = {
            .timestamp_ = 0x7FFFFFFFFFFFFFFF,
            .count_     = 0xFFFFFFFFFFFFFFFF,
        };

        EXPECT_KEY_EQ(key, RecordFileKey::FromBytes(key.ToBytes()));
    }
}

TEST(RecordKeyTest, Compare) {
    {
        EXPECT_LT(
            RecordFileKey::compare(
                RecordFileKey{
                    .timestamp_ = 0xF,
                    .count_     = 0,
                },
                RecordFileKey{
                    .timestamp_ = 0x1111,
                    .count_     = 0,
                }),
            0);
    }

    {
        EXPECT_LT(
            RecordFileKey::compare(
                RecordFileKey{
                    .timestamp_ = -1,
                    .count_     = 0xFFFFFFFFFFFFFFFF,
                },
                RecordFileKey{
                    .timestamp_ = 1,
                    .count_     = 0,
                }),
            0);
    }

    {
        EXPECT_LT(
            RecordFileKey::compare(
                RecordFileKey{
                    .timestamp_ = -1,
                    .count_     = 0xFFFFFFFFFFFFFFFF,
                },
                RecordFileKey{
                    .timestamp_ = 1,
                    .count_     = 0,
                }),
            0);
    }
}

}  // namespace cris::core
