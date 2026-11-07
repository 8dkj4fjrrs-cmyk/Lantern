#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace rolo {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
};

/// Short uppercase name, e.g. "INFO".
std::string_view toString(LogLevel level) noexcept;

/// One log record, handed to every sink.
struct LogMessage {
    LogLevel level = LogLevel::Info;
    std::string category; // e.g. "engine", "editor"
    std::string text;
};

/// A sink receives every message at or above the logger's minimum level.
/// Examples: print to the terminal, append to a file, show in the editor console.
using LogSink = std::function<void(const LogMessage&)>;

/// Routes log messages to any number of sinks.
///
/// Ownership: a Logger is an ordinary object (usually owned by Engine).
/// There is deliberately no global logger, so tests and multiple tools can
/// each have their own without interfering.
class Logger {
public:
    using SinkId = std::size_t;

    /// Registers a sink and returns an id that can later remove it.
    SinkId addSink(LogSink sink);
    void removeSink(SinkId id);

    void setMinimumLevel(LogLevel level) noexcept { m_minimumLevel = level; }
    LogLevel minimumLevel() const noexcept { return m_minimumLevel; }

    void log(LogLevel level, std::string_view category, std::string_view text) const;

    void trace(std::string_view category, std::string_view text) const { log(LogLevel::Trace, category, text); }
    void debug(std::string_view category, std::string_view text) const { log(LogLevel::Debug, category, text); }
    void info(std::string_view category, std::string_view text) const { log(LogLevel::Info, category, text); }
    void warning(std::string_view category, std::string_view text) const { log(LogLevel::Warning, category, text); }
    void error(std::string_view category, std::string_view text) const { log(LogLevel::Error, category, text); }

private:
    struct Entry {
        SinkId id;
        LogSink sink;
    };

    std::vector<Entry> m_sinks;
    SinkId m_nextId = 1;
    LogLevel m_minimumLevel = LogLevel::Info;
};

/// Ready-made sink: writes "[LEVEL] category: text" to stdout (stderr for
/// warnings and errors).
LogSink makeConsoleSink();

} // namespace rolo
