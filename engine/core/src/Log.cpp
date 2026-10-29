#include "lantern/core/Log.hpp"

#include <algorithm>
#include <iostream>
#include <utility>

namespace lantern {

std::string_view toString(LogLevel level) noexcept
{
    switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

Logger::SinkId Logger::addSink(LogSink sink)
{
    const SinkId id = m_nextId++;
    m_sinks.push_back(Entry{id, std::move(sink)});
    return id;
}

void Logger::removeSink(SinkId id)
{
    std::erase_if(m_sinks, [id](const Entry& entry) { return entry.id == id; });
}

void Logger::log(LogLevel level, std::string_view category, std::string_view text) const
{
    if (level < m_minimumLevel) {
        return;
    }

    const LogMessage message{level, std::string(category), std::string(text)};
    for (const Entry& entry : m_sinks) {
        if (entry.sink) {
            entry.sink(message);
        }
    }
}

LogSink makeConsoleSink()
{
    return [](const LogMessage& message) {
        std::ostream& out = (message.level >= LogLevel::Warning) ? std::cerr : std::cout;
        out << '[' << toString(message.level) << "] " << message.category << ": " << message.text << '\n';
    };
}

} // namespace lantern
