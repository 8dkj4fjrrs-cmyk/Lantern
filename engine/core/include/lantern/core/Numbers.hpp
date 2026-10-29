#pragma once

// Converting numbers to and from text, the same way on every computer.
//
// Two traps this avoids:
//   * Locale: in many countries the system writes 1,5 for one and a half.
//     Files must always use a dot, so these functions ignore the locale.
//   * Portability: std::from_chars/to_chars for floating-point numbers are
//     missing from some standard libraries (including Apple's), so they are
//     not used here.

#include <optional>
#include <string>
#include <string_view>

namespace lantern {

/// Reads a whole string as a number ("1.5", "-2", "3e2"). Nothing if the text
/// is not exactly a finite number.
std::optional<double> parseNumber(std::string_view text);

/// The shortest text that reads back as exactly `value` ("0.1", not
/// "0.100000001"; "640", not "640.0").
std::string formatShortest(float value);

/// `value` with at most `decimals` digits after the dot, trailing zeros removed
/// ("1.5", "640", "0.333").
std::string formatDecimals(double value, int decimals);

} // namespace lantern
