#include "core/Amounts.h"

#include "operation/Operation.h"

#include <cctype>
#include <limits>

namespace admin {

pp::Roe<uint64_t> parseAmount(const std::string &text) {
  uint64_t value = 0;
  bool digits = false;
  for (const char c : text) {
    if (std::isdigit(static_cast<unsigned char>(c))) {
      const uint64_t digit = static_cast<uint64_t>(c - '0');
      constexpr uint64_t kMax = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
      if (value > (kMax - digit) / 10) {
        return pp::Error("That amount is too large");
      }
      value = value * 10 + digit;
      digits = true;
    } else if (c != ',' && c != ' ' && c != '_') {
      return pp::Error("An amount is whole units: digits, grouped or not (1,000,000)");
    }
  }
  if (!digits) {
    return pp::Error("Type an amount");
  }
  return value;
}

std::string formatAmount(uint64_t amount) { return pp::operation::groupDigits(amount); }

} // namespace admin
