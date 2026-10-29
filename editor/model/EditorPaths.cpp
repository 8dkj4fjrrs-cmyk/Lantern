#include "EditorPaths.hpp"

#include <cstdlib>

namespace lantern::editor {

std::optional<std::string> realEnvironment(const char* name)
{
    // std::getenv is fine here: read-only, and only called from the main thread.
    if (const char* value = std::getenv(name); value != nullptr && *value != '\0') {
        return std::string(value);
    }
    return std::nullopt;
}

HostOs currentOs() noexcept
{
#if defined(__APPLE__)
    return HostOs::MacOS;
#elif defined(_WIN32)
    return HostOs::Windows;
#else
    return HostOs::Linux;
#endif
}

std::filesystem::path userConfigDirectory(HostOs os, const EnvironmentLookup& env)
{
    switch (os) {
    case HostOs::MacOS:
        if (auto home = env("HOME")) {
            return std::filesystem::path(*home) / "Library" / "Application Support" / "Lantern Engine" / "Editor";
        }
        return {};
    case HostOs::Windows:
        if (auto appData = env("APPDATA")) {
            return std::filesystem::path(*appData) / "Lantern Engine" / "Editor";
        }
        return {};
    case HostOs::Linux:
        if (auto xdg = env("XDG_CONFIG_HOME")) {
            return std::filesystem::path(*xdg) / "lantern-engine" / "editor";
        }
        if (auto home = env("HOME")) {
            return std::filesystem::path(*home) / ".config" / "lantern-engine" / "editor";
        }
        return {};
    }
    return {};
}

std::filesystem::path userConfigDirectory()
{
    return userConfigDirectory(currentOs(), realEnvironment);
}

} // namespace lantern::editor
