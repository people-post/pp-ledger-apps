// Arithmetic and dates done portably (no 128-bit integers, no timegm): the same answers everywhere.
#include "core/Amounts.h"
#include "core/MulDiv.h"

#include <gtest/gtest.h>

#include <limits>
#include <random>

using namespace apps;

TEST(PortableTest, MulDivIsExactPast64Bits) {
  constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();
  EXPECT_EQ(mulDiv(0, 5, 3), 0u);
  EXPECT_EQ(mulDiv(7, 5, 3), 11u);
  EXPECT_EQ(mulDiv(kMax, kMax, kMax), kMax);
  EXPECT_EQ(mulDiv(kMax, 10000, 10000), kMax);
  EXPECT_EQ(mulDiv(kMax - 1, kMax - 2, kMax), kMax - 3) << "(m-1)(m-2)/m, rounded down";
  EXPECT_EQ(mulDiv(1ULL << 63, 6000, 10000), (1ULL << 63) / 10000 * 6000 + ((1ULL << 63) % 10000) * 6000 / 10000);
#ifdef __SIZEOF_INT128__
  std::mt19937_64 random(7);
  for (int i = 0; i < 20000; ++i) {
    uint64_t c = random() | 1;
    uint64_t a = random() % (i % 2 ? c : kMax);
    uint64_t b = random() % c; // b < c: the result fits
    const auto exact = static_cast<uint64_t>(static_cast<unsigned __int128>(a) * b / c);
    ASSERT_EQ(mulDiv(a, b, c), exact) << a << " * " << b << " / " << c;
  }
#endif
  EXPECT_TRUE(productReaches(3, 4, 12));
  EXPECT_FALSE(productReaches(3, 4, 13));
  EXPECT_TRUE(productReaches(kMax, kMax, kMax));
  EXPECT_FALSE(productReaches(0, kMax, 1));
  EXPECT_TRUE(productReaches(0, 5, 0));
}

TEST(PortableTest, DatesAreReadInUtc) {
  EXPECT_EQ(parseUtcDate("1970-01-01"), 0);
  EXPECT_EQ(parseUtcDate("2000-03-01"), 951868800);
  EXPECT_EQ(parseUtcDate(" 2026-10-31 18:00 "), 1793469600);
  EXPECT_EQ(parseUtcDate("2024-02-29"), 1709164800) << "a leap day";
  EXPECT_FALSE(parseUtcDate("31/10/2026"));
  EXPECT_FALSE(parseUtcDate(""));
}
