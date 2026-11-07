#include "rolo/core/Numbers.hpp"

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace rolo {

std::optional<double> parseNumber(std::string_view text)
{
    if (text.empty() || text.front() == ' ' || text.back() == ' ') {
        return std::nullopt;
    }
    std::istringstream in{std::string(text)};
    in.imbue(std::locale::classic());
    double value = 0.0;
    in >> std::noskipws >> value;
    if (in.fail() || in.peek() != std::char_traits<char>::eof() || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

std::string formatShortest(float value)
{
    if (!std::isfinite(value)) {
        return "0";
    }
    for (int precision = 1; precision <= 9; ++precision) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(precision) << value;
        const std::string text = out.str();
        if (const auto back = parseNumber(text); back && static_cast<float>(*back) == value) {
            return text;
        }
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(9) << value;
    return out.str();
}

std::string formatDecimals(double value, int decimals)
{
    if (!std::isfinite(value)) {
        return "0";
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(decimals) << value;
    std::string text = out.str();
    if (text.find('.') != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }
    if (text == "-0") {
        text = "0";
    }
    return text;
}

} // namespace rolo
