#include "ConsoleLog.hpp"

#include <algorithm>
#include <cctype>

namespace rolo::editor {

namespace {

char lower(char c)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool containsIgnoringCase(std::string_view haystack, std::string_view needle)
{
    if (needle.empty()) {
        return true;
    }
    const auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                                [](char a, char b) { return lower(a) == lower(b); });
    return it != haystack.end();
}

} // namespace

bool ConsoleFilter::shows(LogLevel level) const noexcept
{
    switch (level) {
    case LogLevel::Trace: return showTrace;
    case LogLevel::Debug: return showDebug;
    case LogLevel::Info: return showInfo;
    case LogLevel::Warning: return showWarning;
    case LogLevel::Error: return showError;
    }
    return true;
}

bool ConsoleFilter::matches(const ConsoleEntry& entry) const
{
    if (!shows(entry.level)) {
        return false;
    }
    if (search.empty()) {
        return true;
    }
    return containsIgnoringCase(entry.category + ": " + entry.text, search);
}

ConsoleLog::ConsoleLog(std::size_t capacity)
    : m_capacity(std::max<std::size_t>(capacity, 1))
{
}

std::size_t ConsoleLog::levelIndex(LogLevel level) noexcept
{
    return static_cast<std::size_t>(level);
}

void ConsoleLog::add(const LogMessage& message)
{
    const std::chrono::duration<double> age = std::chrono::steady_clock::now() - m_start;

    std::lock_guard lock(m_mutex);
    if (m_entries.size() == m_capacity) {
        --m_counts[levelIndex(m_entries.front().level)];
        m_entries.pop_front();
    }
    m_entries.push_back(ConsoleEntry{m_nextSequence++, message.level, message.category, message.text, age.count()});
    ++m_counts[levelIndex(message.level)];
}

LogSink ConsoleLog::makeSink()
{
    return [this](const LogMessage& message) { add(message); };
}

void ConsoleLog::clear()
{
    std::lock_guard lock(m_mutex);
    m_entries.clear();
    m_counts.fill(0);
}

std::size_t ConsoleLog::size() const
{
    std::lock_guard lock(m_mutex);
    return m_entries.size();
}

std::size_t ConsoleLog::count(LogLevel level) const
{
    std::lock_guard lock(m_mutex);
    return m_counts[levelIndex(level)];
}

std::uint64_t ConsoleLog::totalAdded() const
{
    std::lock_guard lock(m_mutex);
    return m_nextSequence - 1;
}

std::vector<ConsoleEntry> ConsoleLog::snapshot(const ConsoleFilter& filter) const
{
    std::lock_guard lock(m_mutex);
    std::vector<ConsoleEntry> result;
    for (const ConsoleEntry& entry : m_entries) {
        if (filter.matches(entry)) {
            result.push_back(entry);
        }
    }
    return result;
}

std::string ConsoleLog::toText(const ConsoleFilter& filter) const
{
    std::string text;
    for (const ConsoleEntry& entry : snapshot(filter)) {
        text += '[';
        text += toString(entry.level);
        text += "] ";
        text += entry.category;
        text += ": ";
        text += entry.text;
        text += '\n';
    }
    return text;
}

} // namespace rolo::editor
