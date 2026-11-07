#include "rolo/core/Version.hpp"

namespace rolo {

std::string_view versionString() noexcept
{
    return kVersionString;
}

std::string_view runtimeBackendName() noexcept
{
    return kRuntimeBackend;
}

} // namespace rolo
