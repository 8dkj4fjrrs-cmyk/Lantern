#include "EditorPaths.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>

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

std::filesystem::path defaultProjectsDirectory(HostOs os, const EnvironmentLookup& env)
{
    std::optional<std::string> home = os == HostOs::Windows ? env("USERPROFILE") : env("HOME");
    if (!home || home->empty()) {
        return {};
    }
    return std::filesystem::path(*home) / "Documents" / "Lantern Projects";
}

std::filesystem::path defaultProjectsDirectory()
{
    return defaultProjectsDirectory(currentOs(), realEnvironment);
}

bool revealInFileManager(const std::filesystem::path& folder)
{
    // Quoted for the shell, so spaces and quotes in the path are safe.
#if defined(_WIN32)
    std::string quoted = "\"";
    for (char c : folder.string()) {
        if (c != '"') {
            quoted += c;
        }
    }
    quoted += "\"";
    const std::string command = "explorer " + quoted;
#else
    std::string quoted = "'";
    for (char c : folder.string()) {
        quoted += c == '\'' ? std::string("'\\''") : std::string(1, c);
    }
    quoted += "'";
#if defined(__APPLE__)
    const std::string command = "open " + quoted;
#else
    const std::string command = "xdg-open " + quoted + " >/dev/null 2>&1 &";
#endif
#endif
    return std::system(command.c_str()) == 0;
}

} // namespace lantern::editor
