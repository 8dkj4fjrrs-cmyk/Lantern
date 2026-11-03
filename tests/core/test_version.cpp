#include "lantern/core/Version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("Version string matches the numeric version", "[core][version]")
{
    const std::string expected = std::to_string(lantern::kVersionMajor) + "."
                                 + std::to_string(lantern::kVersionMinor) + "."
                                 + std::to_string(lantern::kVersionPatch) + std::string(lantern::kVersionSuffix);
    CHECK(lantern::versionString() == expected);
}

TEST_CASE("The runtime backend name is either none or Axmol", "[core][version]")
{
    const std::string_view name = lantern::runtimeBackendName();
    CHECK((name == "none" || name.starts_with("Axmol ")));
}
