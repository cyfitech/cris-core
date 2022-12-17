#include "cris/core/utils/misc.h"

#include <gtest/gtest.h>

#include <map>
#include <string>

namespace cris::core {
TEST(MiscTest, InverseMapping_NoDuplicate) {
    std::map<int, std::string> original_map{{1, "a"}, {2, "b"}};

    auto inversed_map = InverseMapping(original_map);
    EXPECT_EQ(inversed_map.size(), 2u);

    {
        const auto iter = inversed_map.find("a");
        EXPECT_TRUE(iter != inversed_map.cend());
        EXPECT_EQ(1, iter->second);
    }

    {
        const auto iter = inversed_map.find("b");
        EXPECT_TRUE(iter != inversed_map.cend());
        EXPECT_EQ(2, iter->second);
    }
}

TEST(MiscTest, InverseMapping_HasDuplicate) {
    std::map<int, std::string> original_map{{1, "a"}, {2, "b"}, {3, "a"}};

    EXPECT_DEATH(InverseMapping(original_map), "duplicate value");
}
}  // namespace cris::core
