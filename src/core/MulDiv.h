#pragma once
// Exact integer arithmetic past 64 bits, without a compiler's 128-bit integer type.

#include <cstdint>

namespace apps {

/**
 * floor(a * b / c) for c > 0, exact: the product is formed in 128 bits (two 64-bit halves) and divided
 * by long division. The result must fit in 64 bits (it does whenever b <= c, or a <= c).
 */
inline uint64_t mulDiv(uint64_t a, uint64_t b, uint64_t c) {
  const uint64_t aLo = a & 0xffffffffULL;
  const uint64_t aHi = a >> 32;
  const uint64_t bLo = b & 0xffffffffULL;
  const uint64_t bHi = b >> 32;
  const uint64_t p0 = aLo * bLo;
  const uint64_t p1 = aLo * bHi;
  const uint64_t p2 = aHi * bLo;
  const uint64_t p3 = aHi * bHi;
  const uint64_t mid = (p0 >> 32) + (p1 & 0xffffffffULL) + (p2 & 0xffffffffULL);
  const uint64_t lo = (p0 & 0xffffffffULL) | (mid << 32);
  const uint64_t hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
  uint64_t quotient = 0;
  uint64_t remainder = 0; // always below c
  for (int i = 127; i >= 0; --i) {
    const uint64_t bit = i >= 64 ? (hi >> (i - 64)) & 1ULL : (lo >> i) & 1ULL;
    const bool carry = (remainder >> 63) != 0; // the shift below drops it: the true value is ≥ 2^64 > c
    remainder = (remainder << 1) | bit;
    if (carry || remainder >= c) {
      remainder -= c;
      if (i < 64) {
        quotient |= 1ULL << i;
      }
    }
  }
  return quotient;
}

/** Whether a * b >= limit, without overflowing. */
inline bool productReaches(uint64_t a, uint64_t b, uint64_t limit) {
  if (a == 0 || b == 0) {
    return limit == 0;
  }
  return a >= limit / b + (limit % b != 0 ? 1 : 0);
}

} // namespace apps
