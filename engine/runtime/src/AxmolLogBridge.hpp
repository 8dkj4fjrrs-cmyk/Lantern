#pragma once

// Routes Axmol's internal log messages into a lantern::Logger so that every
// message — ours and the backend's — goes through one place (terminal today,
// the editor console later).
//
// Internal to lantern_runtime: never include this from a public header.

#include "lantern/core/Log.hpp"

#include <axmol.h>

#include <string_view>

namespace lantern::detail {

/// Maps an Axmol log level onto Lantern's levels.
LogLevel toLanternLevel(ax::LogLevel level) noexcept;

/// Removes trailing newline characters (Axmol messages end with '\n').
std::string_view trimTrailingNewlines(std::string_view text) noexcept;

/// While alive, all Axmol log output goes to `logger` under category "axmol".
/// Restores Axmol's default output when destroyed (RAII).
class AxmolLogBridge final : public ax::ILogOutput {
public:
    explicit AxmolLogBridge(const Logger& logger);
    ~AxmolLogBridge() override;

    AxmolLogBridge(const AxmolLogBridge&) = delete;
    AxmolLogBridge& operator=(const AxmolLogBridge&) = delete;

    void write(ax::LogItem& item, const char* tag) override;

private:
    const Logger& m_logger;
};

} // namespace lantern::detail
