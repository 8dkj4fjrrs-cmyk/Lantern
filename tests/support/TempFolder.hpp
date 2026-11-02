// Copyright (c) 2026 Oscar Hird. MIT licence: see LICENSE.
#pragma once

// A folder name of its own for one test run. ctest runs test cases in
// parallel processes, so two cases sharing a fixed folder name would delete
// each other's files.

#include <filesystem>
#include <random>
#include <string>
#include <string_view>

namespace lantern::test {

inline std::filesystem::path uniqueTempFolder(std::string_view name)
{
    std::random_device random;
    return std::filesystem::temp_directory_path() / "lantern-tests" /
           (std::string(name) + "-" + std::to_string(random()));
}

} // namespace lantern::test
