#include "lantern/core/Version.hpp"

namespace lantern {

std::string_view versionString() noexcept
{
    return kVersionString;
}

std::string_view runtimeBackendName() noexcept
{
    // Becomes the real backend name once Milestone 1 links one in.
    return "none";
}

} // namespace lantern
