#include "AxmolLogBridge.hpp"

namespace rolo::detail {

LogLevel toRoloLevel(ax::LogLevel level) noexcept
{
    switch (level) {
    case ax::LogLevel::Verbose: return LogLevel::Trace;
    case ax::LogLevel::Debug: return LogLevel::Debug;
    case ax::LogLevel::Info: return LogLevel::Info;
    case ax::LogLevel::Warn: return LogLevel::Warning;
    case ax::LogLevel::Error: return LogLevel::Error;
    case ax::LogLevel::Silent: return LogLevel::Info; // Axmol uses this for unlevelled print()
    }
    return LogLevel::Info;
}

std::string_view trimTrailingNewlines(std::string_view text) noexcept
{
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

AxmolLogBridge::AxmolLogBridge(const Logger& logger)
    : m_logger(logger)
{
    // We print our own "[LEVEL] category:" prefix, so ask Axmol for the bare message.
    ax::setLogFmtFlag(ax::LogFmtFlag::Null);
    ax::setLogOutput(this);
}

AxmolLogBridge::~AxmolLogBridge()
{
    ax::setLogOutput(nullptr); // back to Axmol's default (stdout)
}

void AxmolLogBridge::write(ax::LogItem& item, const char* /*tag*/)
{
    m_logger.log(toRoloLevel(item.level()), "axmol", trimTrailingNewlines(item.message()));
}

} // namespace rolo::detail
