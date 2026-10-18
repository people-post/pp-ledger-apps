#include "core/Amounts.h"
#include <ctime>
#include <sstream>
#include <iomanip>

#include "operation/Operation.h"

#include <cctype>
#include <limits>

namespace apps {

namespace {

/** Days from 1970-01-01 to a date of the proleptic Gregorian calendar (H. Hinnant's days_from_civil). */
int64_t daysFromCivil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2 ? 1 : 0;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const auto yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

/** "2026-10-31" or "2026-10-31 18:00" (UTC) → Unix seconds. */
std::optional<int64_t> parseDateText(const std::string &text) {
  std::tm tm{};
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return std::nullopt;
  }
  std::istringstream in(text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1));
  in >> std::get_time(&tm, "%Y-%m-%d");
  if (in.fail()) {
    return std::nullopt;
  }
  if (!in.eof()) {
    in >> std::ws;
    if (!in.eof()) {
      in >> std::get_time(&tm, "%H:%M");
      if (in.fail()) {
        return std::nullopt;
      }
    }
  }
  if (tm.tm_mon < 0 || tm.tm_mon > 11 || tm.tm_mday < 1 || tm.tm_mday > 31) {
    return std::nullopt;
  }
  const int64_t days = daysFromCivil(tm.tm_year + 1900, static_cast<unsigned>(tm.tm_mon + 1),
                                     static_cast<unsigned>(tm.tm_mday));
  return days * 86400 + tm.tm_hour * 3600 + tm.tm_min * 60;
}

} // namespace

std::optional<int64_t> parseUtcDate(const std::string &text) { return parseDateText(text); }

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

} // namespace apps
