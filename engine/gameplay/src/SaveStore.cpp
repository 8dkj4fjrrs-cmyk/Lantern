#include "rolo/gameplay/SaveStore.hpp"

#include "rolo/content/GameState.hpp"
#include "rolo/core/Files.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace rolo {

namespace {

bool isValidSlot(std::string_view slot)
{
    return !slot.empty() && slot.size() <= 64 && std::all_of(slot.begin(), slot.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

std::optional<std::string> env(const char* name)
{
    return environmentVariable(name);
}

} // namespace

FolderSaveStore::FolderSaveStore(std::filesystem::path folder)
    : m_folder(std::move(folder))
{
}

std::filesystem::path FolderSaveStore::pathOf(std::string_view slot) const
{
    if (!isValidSlot(slot)) {
        return {};
    }
    return m_folder / (std::string(slot) + std::string(SaveFormat::kExtension));
}

Status FolderSaveStore::write(std::string_view slot, std::string_view text)
{
    const std::filesystem::path file = pathOf(slot);
    if (file.empty()) {
        return Status::failure("\"" + std::string(slot) + "\" is not a valid save slot name (use letters, digits, _ and -).");
    }
    std::error_code ec;
    std::filesystem::create_directories(m_folder, ec);
    // Write a new file, then replace the old one: a crash while saving
    // leaves the previous save intact.
    const std::filesystem::path temp = file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) {
            return Status::failure("Could not write the save file " + temp.string() + ".");
        }
    }
    std::filesystem::rename(temp, file, ec);
    if (ec) {
        return Status::failure("Could not save to " + file.string() + ": " + ec.message());
    }
    return Status::success();
}

std::optional<std::string> FolderSaveStore::read(std::string_view slot) const
{
    const std::filesystem::path file = pathOf(slot);
    if (file.empty()) {
        return std::nullopt;
    }
    return readTextFile(file);
}

std::filesystem::path FolderSaveStore::userFolder(std::string_view gameName)
{
    std::string name(gameName.empty() ? "Rolo Game" : gameName);
    for (char& c : name) {
        if (std::string_view("/\\:*?\"<>|").find(c) != std::string_view::npos) {
            c = '_';
        }
    }
#if defined(_WIN32)
    if (const auto appData = env("APPDATA")) {
        return std::filesystem::path(*appData) / name / "Saves";
    }
#elif defined(__APPLE__)
    if (const auto home = env("HOME")) {
        return std::filesystem::path(*home) / "Library" / "Application Support" / name / "Saves";
    }
#else
    if (const auto xdg = env("XDG_DATA_HOME")) {
        return std::filesystem::path(*xdg) / name / "saves";
    }
    if (const auto home = env("HOME")) {
        return std::filesystem::path(*home) / ".local" / "share" / name / "saves";
    }
#endif
    return {};
}

} // namespace rolo
