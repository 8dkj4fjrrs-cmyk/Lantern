#pragma once

// The editor console's message store: keeps the most recent log messages so
// the Console panel can show, filter and copy them.
//
// Plain C++ (no UI toolkit), so it can be unit-tested and would survive a
// change of UI library.

#include "lantern/core/Log.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace lantern::editor {

struct ConsoleEntry {
    std::uint64_t sequence = 0;    // 1 for the first message ever added
    LogLevel level = LogLevel::Info;
    std::string category;
    std::string text;
    double secondsSinceStart = 0.0;
};

/// Which entries the Console panel shows.
struct ConsoleFilter {
    bool showTrace = false;
    bool showDebug = true;
    bool showInfo = true;
    bool showWarning = true;
    bool showError = true;
    std::string search; // case-insensitive, matched against "category: text"

    bool shows(LogLevel level) const noexcept;
    bool matches(const ConsoleEntry& entry) const;
};

class ConsoleLog {
public:
    /// Keeps at most `capacity` entries; the oldest are dropped first.
    explicit ConsoleLog(std::size_t capacity = 5000);

    /// Adds one message. Safe to call from any thread (the backend may log
    /// from worker threads).
    void add(const LogMessage& message);

    /// A sink to register with a Logger. The ConsoleLog must outlive the
    /// registration: remove the sink before destroying this object.
    LogSink makeSink();

    void clear();

    std::size_t size() const;
    std::size_t capacity() const noexcept { return m_capacity; }

    /// How many entries of this level are currently stored.
    std::size_t count(LogLevel level) const;

    /// Total messages ever added (including dropped and cleared ones).
    std::uint64_t totalAdded() const;

    /// The stored entries that pass `filter`, oldest first.
    std::vector<ConsoleEntry> snapshot(const ConsoleFilter& filter) const;

    /// Same entries as text, one per line: "[LEVEL] category: text".
    std::string toText(const ConsoleFilter& filter) const;

private:
    static std::size_t levelIndex(LogLevel level) noexcept;

    mutable std::mutex m_mutex;
    std::deque<ConsoleEntry> m_entries;
    std::array<std::size_t, 5> m_counts{};
    std::size_t m_capacity;
    std::uint64_t m_nextSequence = 1;
    std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
};

} // namespace lantern::editor
