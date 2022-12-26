#include "cris/core/msg_recorder/record_file.h"

#include "gtest/gtest.h"

namespace cris::core {

// Use macros to keep line information.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-errors)
#define EXPECT_KEY_EQ(lhs, rhs)                              \
    do {                                                     \
        EXPECT_EQ((lhs).timestamp_ns_, (rhs).timestamp_ns_); \
        EXPECT_EQ((lhs).count_, (rhs).count_);               \
        EXPECT_EQ(RecordFileKey::compare(lhs, rhs), 0);      \
    } while (0)

TEST(RecordKeyTest, Convert) {
    {
        RecordFileKey key = {
            .timestamp_ns_ = 1234567,
            .count_        = 0,
        };

        const auto key_from_bytes_opt = RecordFileKey::FromBytes(key.ToBytes());
        if (!key_from_bytes_opt) {
            FAIL();
        }
        EXPECT_KEY_EQ(key, *key_from_bytes_opt);
    }

    {
        RecordFileKey key = {
            .timestamp_ns_ = 0x7FFFFFFFFFFFFFFF,
            .count_        = 0xFFFFFFFFFFFFFFFF,
        };

        const auto key_from_bytes_opt = RecordFileKey::FromBytes(key.ToBytes());
        if (!key_from_bytes_opt) {
            FAIL();
        }
        EXPECT_KEY_EQ(key, *key_from_bytes_opt);
    }
}

TEST(RecordKeyTest, Compare) {
    {
        EXPECT_LT(
            RecordFileKey::compare(
                RecordFileKey{
                    .timestamp_ns_ = 0xF,
                    .count_        = 0,
                },
                RecordFileKey{
                    .timestamp_ns_ = 0x1111,
                    .count_        = 0,
                }),
            0);
    }

    {
        EXPECT_LT(
            RecordFileKey::compare(
                RecordFileKey{
                    .timestamp_ns_ = -1,
                    .count_        = 0xFFFFFFFFFFFFFFFF,
                },
                RecordFileKey{
                    .timestamp_ns_ = 1,
                    .count_        = 0,
                }),
            0);
    }

    {
        EXPECT_LT(
            RecordFileKey::compare(
                RecordFileKey{
                    .timestamp_ns_ = -1,
                    .count_        = 0xFFFFFFFFFFFFFFFF,
                },
                RecordFileKey{
                    .timestamp_ns_ = 1,
                    .count_        = 0,
                }),
            0);
    }
}

}  // namespace cris::core
