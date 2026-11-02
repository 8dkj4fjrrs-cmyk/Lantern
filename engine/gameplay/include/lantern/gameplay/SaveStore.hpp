#pragma once

// SaveStore: where a game's save slots are kept.
//
// A slot is a name such as "quick" or "slot1"; its save file is one .lsave
// text file (see GameState.hpp). The player keeps saves in the user's
// application data folder, the editor's play mode in its own settings folder,
// so playing in the editor never touches the player's saves.

#include "lantern/core/Engine.hpp"   // Status

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace lantern {

class SaveStore {
public:
    virtual ~SaveStore() = default;
    virtual Status write(std::string_view slot, std::string_view text) = 0;
    virtual std::optional<std::string> read(std::string_view slot) const = 0;
};

/// Keeps each slot as `<folder>/<slot>.lsave`. Slot names may use letters,
/// digits, _ and - only, so they are safe file names everywhere.
class FolderSaveStore final : public SaveStore {
public:
    explicit FolderSaveStore(std::filesystem::path folder);
    Status write(std::string_view slot, std::string_view text) override;
    std::optional<std::string> read(std::string_view slot) const override;
    const std::filesystem::path& folder() const noexcept { return m_folder; }
    /// The file a slot is kept in (empty path for an invalid slot name).
    std::filesystem::path pathOf(std::string_view slot) const;

    /// The per-user folder for a game's saves:
    /// macOS ~/Library/Application Support/<game>/Saves,
    /// Windows %APPDATA%\<game>\Saves, Linux ~/.local/share/<game>/saves
    /// (or $XDG_DATA_HOME). Empty if the home folder is unknown.
    static std::filesystem::path userFolder(std::string_view gameName);

private:
    std::filesystem::path m_folder;
};

} // namespace lantern
