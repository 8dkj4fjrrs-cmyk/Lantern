#include "rolo/core/Version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("Version string matches the numeric version", "[core][version]")
{
    const std::string expected = std::to_string(rolo::kVersionMajor) + "."
                                 + std::to_string(rolo::kVersionMinor) + "."
                                 + std::to_string(rolo::kVersionPatch) + std::string(rolo::kVersionSuffix);
    CHECK(rolo::versionString() == expected);
}

TEST_CASE("The runtime backend name is either none or Axmol", "[core][version]")
{
    const std::string_view name = rolo::runtimeBackendName();
    CHECK((name == "none" || name.starts_with("Axmol ")));
}
