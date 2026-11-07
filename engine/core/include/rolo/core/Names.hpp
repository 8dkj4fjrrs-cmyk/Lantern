#pragma once

// Names Rolo writes into files, and the older names it still reads.
//
// Rolo Engine was called Lantern Engine until version 0.9.1. Files saved by
// Lantern name their format and component types with a "lantern." prefix
// ("lantern.scene", "lantern.Transform"), and projects are "project.lantern".
// Rolo writes "rolo." names and "project.rolo", and reads both, so projects
// made with Lantern keep opening; saving them writes the new names.

#include <optional>
#include <string>
#include <string_view>

namespace rolo {

/// The prefix of the names Lantern wrote, and Rolo's.
inline constexpr std::string_view kLegacyPrefix = "lantern.";
inline constexpr std::string_view kNamePrefix = "rolo.";

/// True if a file's "format" field names `expected` (e.g. "rolo.scene"),
/// either as written now or as Lantern wrote it ("lantern.scene").
bool isFormatName(std::string_view written, std::string_view expected);

/// The current name for one Lantern wrote: "lantern.Transform" gives
/// "rolo.Transform". Nothing if `name` is not a Lantern name.
std::optional<std::string> upgradeLegacyName(std::string_view name);

} // namespace rolo
