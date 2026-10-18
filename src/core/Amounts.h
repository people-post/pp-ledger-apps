#pragma once
// Amounts as people type and read them: the chain's integer units, digits grouped (AD009).

#include "common/Error.h"

#include <cstdint>
#include <optional>
#include <string>

namespace apps {

/** "1,000,000", "1 000 000", "1_000_000" or "1000000": refused if anything else, or above 2^63-1. */
pp::Roe<uint64_t> parseAmount(const std::string &text);
/** "1,000,000". */
std::string formatAmount(uint64_t amount);

/** "2026-10-31" or "2026-10-31 18:00", in UTC, as Unix seconds; none when it is not such a date. */
std::optional<int64_t> parseUtcDate(const std::string &text);

} // namespace apps
