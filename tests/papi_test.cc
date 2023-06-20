#include "cris/core/utils/papi.h"

#include <gtest/gtest.h>

#include <numeric>
#include <vector>

using namespace std;

namespace cris::core {

/* Disable for ASAN due to a known leak in PAPI.
 * - https://sourceforge.net/p/perfmon2/bugs/17/
 * - https://github.com/cyfitech/cris-core/pull/226
 *
 * TODO(xkszltl): Suppress warnings from PAPI instead.
 */
#ifdef __has_feature
#if !__has_feature(address_sanitizer)
TEST(PAPI, RAII) {
    PAPIStat();
}

TEST(PAPI, Long) {
    const PAPIStat stat;

    const int        round = 100000000;
    vector<unsigned> dat(round);
    iota(dat.begin(), dat.end(), 0);

    unsigned acc = 0;
    for (const auto i : dat) {
        acc += dat[i] * 3;
    }
    EXPECT_EQ(acc, static_cast<unsigned>((static_cast<long long>(round - 1) * round / 2 * 3) & 0xFFFFFFFFll));
}
#endif
#endif

}  // namespace cris::core
