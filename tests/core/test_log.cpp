#include "rolo/core/Log.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using rolo::LogLevel;
using rolo::Logger;
using rolo::LogMessage;

TEST_CASE("Messages reach every registered sink", "[core][log]")
{
    Logger logger;
    std::vector<LogMessage> a;
    std::vector<LogMessage> b;
    logger.addSink([&a](const LogMessage& m) { a.push_back(m); });
    logger.addSink([&b](const LogMessage& m) { b.push_back(m); });

    logger.info("test", "hello");

    REQUIRE(a.size() == 1);
    REQUIRE(b.size() == 1);
    CHECK(a[0].level == LogLevel::Info);
    CHECK(a[0].category == "test");
    CHECK(a[0].text == "hello");
}

TEST_CASE("Messages below the minimum level are dropped", "[core][log]")
{
    Logger logger;
    std::vector<LogMessage> received;
    logger.addSink([&received](const LogMessage& m) { received.push_back(m); });

    logger.setMinimumLevel(LogLevel::Warning);
    logger.debug("test", "too quiet");
    logger.info("test", "still too quiet");
    logger.warning("test", "loud enough");
    logger.error("test", "definitely loud enough");

    REQUIRE(received.size() == 2);
    CHECK(received[0].level == LogLevel::Warning);
    CHECK(received[1].level == LogLevel::Error);
}

TEST_CASE("A removed sink stops receiving messages", "[core][log]")
{
    Logger logger;
    int kept = 0;
    int removed = 0;
    logger.addSink([&kept](const LogMessage&) { ++kept; });
    const Logger::SinkId id = logger.addSink([&removed](const LogMessage&) { ++removed; });

    logger.info("test", "one");
    logger.removeSink(id);
    logger.info("test", "two");

    CHECK(kept == 2);
    CHECK(removed == 1);
}

TEST_CASE("Separate loggers do not share sinks", "[core][log]")
{
    Logger first;
    Logger second;
    int count = 0;
    first.addSink([&count](const LogMessage&) { ++count; });

    second.info("test", "should not reach the first logger");
    CHECK(count == 0);
}

TEST_CASE("Log levels have short names", "[core][log]")
{
    CHECK(rolo::toString(LogLevel::Info) == "INFO");
    CHECK(rolo::toString(LogLevel::Warning) == "WARN");
    CHECK(rolo::toString(LogLevel::Error) == "ERROR");
}
