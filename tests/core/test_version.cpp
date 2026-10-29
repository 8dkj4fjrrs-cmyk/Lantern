#include "lantern/core/Version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("Version string matches the numeric version", "[core][version]")
{
    const std::string expected = std::to_string(lantern::kVersionMajor) + "."
                                 + std::to_string(lantern::kVersionMinor) + "."
                                 + std::to_string(lantern::kVersionPatch);
    CHECK(lantern::versionString() == expected);
}

TEST_CASE("No runtime backend is linked in Milestone 0", "[core][version]")
{
    CHECK(lantern::runtimeBackendName() == "none");
}
