#include "AxmolLogBridge.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using lantern::LogLevel;
using lantern::detail::toLanternLevel;
using lantern::detail::trimTrailingNewlines;

TEST_CASE("Axmol log levels map onto Lantern levels", "[runtime][log]")
{
    CHECK(toLanternLevel(ax::LogLevel::Verbose) == LogLevel::Trace);
    CHECK(toLanternLevel(ax::LogLevel::Debug) == LogLevel::Debug);
    CHECK(toLanternLevel(ax::LogLevel::Info) == LogLevel::Info);
    CHECK(toLanternLevel(ax::LogLevel::Warn) == LogLevel::Warning);
    CHECK(toLanternLevel(ax::LogLevel::Error) == LogLevel::Error);
}

TEST_CASE("Trailing newlines are removed from backend messages", "[runtime][log]")
{
    CHECK(trimTrailingNewlines("hello\n") == "hello");
    CHECK(trimTrailingNewlines("hello\r\n\n") == "hello");
    CHECK(trimTrailingNewlines("no newline") == "no newline");
    CHECK(trimTrailingNewlines("\n").empty());
    CHECK(trimTrailingNewlines("").empty());
}

TEST_CASE("While the bridge exists, Axmol log messages reach the Lantern logger", "[runtime][log]")
{
    lantern::Logger logger;
    logger.setMinimumLevel(LogLevel::Trace);
    std::vector<lantern::LogMessage> received;
    logger.addSink([&received](const lantern::LogMessage& m) { received.push_back(m); });

    {
        lantern::detail::AxmolLogBridge bridge(logger);
        AXLOGW("disk is {} percent full", 93);
    }
    // After the bridge is destroyed, messages go back to Axmol's own output.
    AXLOGW("this one should not reach our logger");

    REQUIRE(received.size() == 1);
    CHECK(received[0].category == "axmol");
    CHECK(received[0].level == LogLevel::Warning);
    CHECK(received[0].text == "disk is 93 percent full");
}
