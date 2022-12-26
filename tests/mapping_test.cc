#include "cris/core/utils/mapping.h"

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

namespace cris::core {
TEST(MiscTest, InverseMapping_InjectiveMapping) {
    std::unordered_map<int, std::string> original_map{{1, "a"}, {2, "b"}};

    auto inversed_map = InverseMapping(original_map);
    EXPECT_EQ(inversed_map.size(), original_map.size());

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

TEST(MiscTest, InverseMapping_NonInjectiveMapping) {
    std::unordered_map<int, std::string> original_map{{1, "a"}, {2, "b"}, {3, "a"}};

    // EXPECT_THROW may contain `goto`.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
    EXPECT_THROW(InverseMapping(original_map), std::logic_error);
}
}  // namespace cris::core
