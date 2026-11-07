#include "rolo/core/Names.hpp"

namespace rolo {

std::optional<std::string> upgradeLegacyName(std::string_view name)
{
    if (name.substr(0, kLegacyPrefix.size()) != kLegacyPrefix || name.size() == kLegacyPrefix.size()) {
        return std::nullopt;
    }
    return std::string(kNamePrefix) + std::string(name.substr(kLegacyPrefix.size()));
}

bool isFormatName(std::string_view written, std::string_view expected)
{
    if (written == expected) {
        return true;
    }
    const auto upgraded = upgradeLegacyName(written);
    return upgraded && *upgraded == expected;
}

} // namespace rolo
