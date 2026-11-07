#include "ConsoleLog.hpp"

#include <catch2/catch_test_macros.hpp>

#include <thread>
#include <vector>

using rolo::LogLevel;
using rolo::LogMessage;
using rolo::editor::ConsoleFilter;
using rolo::editor::ConsoleLog;

namespace {
LogMessage msg(LogLevel level, const char* category, const char* text)
{
    return LogMessage{level, category, text};
}
} // namespace

TEST_CASE("Messages are stored in order with increasing sequence numbers", "[editor][console]")
{
    ConsoleLog log;
    log.add(msg(LogLevel::Info, "engine", "first"));
    log.add(msg(LogLevel::Warning, "axmol", "second"));

    const auto entries = log.snapshot(ConsoleFilter{});
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].text == "first");
    CHECK(entries[1].text == "second");
    CHECK(entries[0].sequence == 1);
    CHECK(entries[1].sequence == 2);
    CHECK(entries[1].category == "axmol");
}

TEST_CASE("The oldest messages are dropped when the log is full", "[editor][console]")
{
    ConsoleLog log(3);
    for (int i = 0; i < 5; ++i) {
        log.add(msg(i == 0 ? LogLevel::Error : LogLevel::Info, "t", std::to_string(i).c_str()));
    }
    const auto entries = log.snapshot(ConsoleFilter{});
    REQUIRE(entries.size() == 3);
    CHECK(entries.front().text == "2");
    CHECK(log.totalAdded() == 5);
    CHECK(log.count(LogLevel::Error) == 0); // the error was dropped, so it is no longer counted
    CHECK(log.count(LogLevel::Info) == 3);
}

TEST_CASE("Level counts and clearing", "[editor][console]")
{
    ConsoleLog log;
    log.add(msg(LogLevel::Error, "a", "x"));
    log.add(msg(LogLevel::Error, "a", "y"));
    log.add(msg(LogLevel::Warning, "a", "z"));
    CHECK(log.count(LogLevel::Error) == 2);
    CHECK(log.count(LogLevel::Warning) == 1);

    log.clear();
    CHECK(log.size() == 0);
    CHECK(log.count(LogLevel::Error) == 0);
    CHECK(log.totalAdded() == 3); // history counter is not reset
}

TEST_CASE("The filter hides levels and matches text case-insensitively", "[editor][console]")
{
    ConsoleLog log;
    log.add(msg(LogLevel::Debug, "runtime", "GPU: Apple M2"));
    log.add(msg(LogLevel::Info, "editor", "Settings folder ready"));
    log.add(msg(LogLevel::Error, "axmol", "Shader failed"));

    ConsoleFilter filter;
    filter.showDebug = false;
    CHECK(log.snapshot(filter).size() == 2);

    filter = ConsoleFilter{};
    filter.search = "shader";
    auto found = log.snapshot(filter);
    REQUIRE(found.size() == 1);
    CHECK(found[0].level == LogLevel::Error);

    filter.search = "AXMOL:"; // category is searchable too
    CHECK(log.snapshot(filter).size() == 1);
}

TEST_CASE("Trace messages are hidden by default", "[editor][console]")
{
    CHECK_FALSE(ConsoleFilter{}.shows(LogLevel::Trace));
    CHECK(ConsoleFilter{}.shows(LogLevel::Error));
}

TEST_CASE("Copy text has one line per visible message", "[editor][console]")
{
    ConsoleLog log;
    log.add(msg(LogLevel::Info, "engine", "hello"));
    log.add(msg(LogLevel::Error, "axmol", "oops"));
    CHECK(log.toText(ConsoleFilter{}) == "[INFO] engine: hello\n[ERROR] axmol: oops\n");
}

TEST_CASE("The log sink feeds a Logger into the console", "[editor][console]")
{
    ConsoleLog console;
    rolo::Logger logger;
    const auto id = logger.addSink(console.makeSink());
    logger.warning("test", "through the logger");
    logger.removeSink(id);
    REQUIRE(console.size() == 1);
    CHECK(console.snapshot(ConsoleFilter{})[0].text == "through the logger");
}

TEST_CASE("Messages can be added from several threads at once", "[editor][console]")
{
    ConsoleLog log(100000);
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&log] {
            for (int i = 0; i < 1000; ++i) {
                log.add(msg(LogLevel::Info, "thread", "message"));
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(log.size() == 4000);
    CHECK(log.count(LogLevel::Info) == 4000);
}
