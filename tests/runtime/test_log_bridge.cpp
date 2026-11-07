#include "AxmolLogBridge.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using rolo::LogLevel;
using rolo::detail::toRoloLevel;
using rolo::detail::trimTrailingNewlines;

TEST_CASE("Axmol log levels map onto Rolo levels", "[runtime][log]")
{
    CHECK(toRoloLevel(ax::LogLevel::Verbose) == LogLevel::Trace);
    CHECK(toRoloLevel(ax::LogLevel::Debug) == LogLevel::Debug);
    CHECK(toRoloLevel(ax::LogLevel::Info) == LogLevel::Info);
    CHECK(toRoloLevel(ax::LogLevel::Warn) == LogLevel::Warning);
    CHECK(toRoloLevel(ax::LogLevel::Error) == LogLevel::Error);
}

TEST_CASE("Trailing newlines are removed from backend messages", "[runtime][log]")
{
    CHECK(trimTrailingNewlines("hello\n") == "hello");
    CHECK(trimTrailingNewlines("hello\r\n\n") == "hello");
    CHECK(trimTrailingNewlines("no newline") == "no newline");
    CHECK(trimTrailingNewlines("\n").empty());
    CHECK(trimTrailingNewlines("").empty());
}

TEST_CASE("While the bridge exists, Axmol log messages reach the Rolo logger", "[runtime][log]")
{
    rolo::Logger logger;
    logger.setMinimumLevel(LogLevel::Trace);
    std::vector<rolo::LogMessage> received;
    logger.addSink([&received](const rolo::LogMessage& m) { received.push_back(m); });

    {
        rolo::detail::AxmolLogBridge bridge(logger);
        AXLOGW("disk is {} percent full", 93);
    }
    // After the bridge is destroyed, messages go back to Axmol's own output.
    AXLOGW("this one should not reach our logger");

    REQUIRE(received.size() == 1);
    CHECK(received[0].category == "axmol");
    CHECK(received[0].level == LogLevel::Warning);
    CHECK(received[0].text == "disk is 93 percent full");
}
