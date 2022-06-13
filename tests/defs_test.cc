#include "cris/core/defs.h"

#include "gtest/gtest.h"

namespace cris::core {

class TestType1 {};

template<class T>
class TestType2 {};

TEST(DefsTest, TypeNameTest) {
    EXPECT_EQ(GetTypeName<int>(), "int");
    EXPECT_EQ(GetTypeName<TestType1>(), "cris::core::TestType1");
    EXPECT_EQ(GetTypeName<TestType2<int>>(), "cris::core::TestType2<int>");

    using TestType3 = TestType1;
    EXPECT_EQ(GetTypeName<TestType3>(), GetTypeName<TestType1>());
}

}  // namespace cris::core
