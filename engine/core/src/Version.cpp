#include "lantern/core/Version.hpp"

namespace lantern {

std::string_view versionString() noexcept
{
    return kVersionString;
}

std::string_view runtimeBackendName() noexcept
{
    return kRuntimeBackend;
}

} // namespace lantern
