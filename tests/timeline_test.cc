#include "cris/core/utils/timeline.h"

#include <gtest/gtest.h>

#include <numeric>
#include <vector>

using namespace std;

namespace cris::core {

TEST(Timeline, RAII) {
    CRIS_TIMELINE_LOCAL("root");
    CRIS_FUNC_TIMELINE();
    {
        Timeline::LocalSession sess("basic");
        Timeline::Region("test");
    }
    {
        Timeline::Session sess("global");
        {
            Timeline::Region("foo");
            Timeline::Region("bar");
            CRIS_FUNC_TIMELINE();
            CRIS_FUNC_TIMELINE();
        }
    }
    {
        Timeline::LocalSession sess("local");
        {
            Timeline::Region("x");
            {
                Timeline::Region region_y("y");
                {
                    Timeline::Region region_u("u");
                    []() {
                        Timeline::Region("lambda");
                    }();
                }
                Timeline::Region("v");
            }
            Timeline::Region("z");
        }
    }
    { Timeline::LocalSession sess("global"); }
}

}  // namespace cris::core
