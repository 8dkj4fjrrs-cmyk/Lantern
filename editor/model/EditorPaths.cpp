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

std::vector<std::filesystem::path> uiFontCandidates(HostOs os, const EnvironmentLookup& env)
{
    switch (os) {
    case HostOs::MacOS:
        return {
            "/System/Library/Fonts/SFNS.ttf",
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/Library/Fonts/Arial.ttf",
        };
    case HostOs::Windows: {
        const std::filesystem::path fonts =
            std::filesystem::path(env("WINDIR").value_or("C:\\Windows")) / "Fonts";
        return {fonts / "segoeui.ttf", fonts / "arial.ttf"};
    }
    case HostOs::Linux:
        return {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        };
    }
    return {};
}

std::filesystem::path findUiFont()
{
    for (const std::filesystem::path& candidate : uiFontCandidates(currentOs(), realEnvironment)) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

} // namespace lantern::editor
