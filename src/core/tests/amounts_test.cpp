#include "core/Amounts.h"

#include <gtest/gtest.h>

using namespace apps;

TEST(AmountsTest, AmountsAreWholeUnitsGroupedOrNot) {
  EXPECT_EQ(parseAmount("1,000,000").value(), 1'000'000u);
  EXPECT_EQ(parseAmount("1 000 000").value(), 1'000'000u);
  EXPECT_EQ(parseAmount("1_000").value(), 1'000u);
  EXPECT_EQ(parseAmount("42").value(), 42u);
  EXPECT_FALSE(parseAmount(""));
  EXPECT_FALSE(parseAmount("1.5"));
  EXPECT_FALSE(parseAmount("-3"));
  EXPECT_FALSE(parseAmount("12abc"));
  EXPECT_FALSE(parseAmount("9223372036854775808")); // 2^63
  EXPECT_EQ(parseAmount("9223372036854775807").value(), 9223372036854775807u);
  EXPECT_EQ(formatAmount(1'234'567), "1,234,567");
}
