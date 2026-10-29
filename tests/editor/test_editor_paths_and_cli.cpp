#include "CommandLine.hpp"
#include "EditorPaths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>

using namespace lantern::editor;

namespace {

EnvironmentLookup fakeEnvironment(std::map<std::string, std::string> values)
{
    return [values = std::move(values)](const char* name) -> std::optional<std::string> {
        if (auto it = values.find(name); it != values.end()) {
            return it->second;
        }
        return std::nullopt;
    };
}

EditorParseResult parse(std::initializer_list<std::string_view> args)
{
    const std::vector<std::string_view> list(args);
    return parseEditorCommandLine(list);
}

} // namespace

TEST_CASE("Settings folder follows each operating system's convention", "[editor][paths]")
{
    const auto env = fakeEnvironment({{"HOME", "/Users/oscar"}, {"APPDATA", "C:/Users/oscar/AppData/Roaming"}});
    CHECK(userConfigDirectory(HostOs::MacOS, env)
          == std::filesystem::path("/Users/oscar/Library/Application Support/Lantern Engine/Editor"));
    CHECK(userConfigDirectory(HostOs::Windows, env)
          == std::filesystem::path("C:/Users/oscar/AppData/Roaming") / "Lantern Engine" / "Editor");
    CHECK(userConfigDirectory(HostOs::Linux, env)
          == std::filesystem::path("/Users/oscar/.config/lantern-engine/editor"));
}

TEST_CASE("Linux respects XDG_CONFIG_HOME", "[editor][paths]")
{
    const auto env = fakeEnvironment({{"HOME", "/home/oscar"}, {"XDG_CONFIG_HOME", "/data/config"}});
    CHECK(userConfigDirectory(HostOs::Linux, env) == std::filesystem::path("/data/config/lantern-engine/editor"));
}

TEST_CASE("No home folder gives an empty path instead of guessing", "[editor][paths]")
{
    const auto env = fakeEnvironment({});
    CHECK(userConfigDirectory(HostOs::MacOS, env).empty());
    CHECK(userConfigDirectory(HostOs::Linux, env).empty());
    CHECK(userConfigDirectory(HostOs::Windows, env).empty());
}

TEST_CASE("Every OS has at least one UI font candidate", "[editor][paths]")
{
    const auto env = fakeEnvironment({});
    CHECK_FALSE(uiFontCandidates(HostOs::MacOS, env).empty());
    CHECK_FALSE(uiFontCandidates(HostOs::Windows, env).empty());
    CHECK_FALSE(uiFontCandidates(HostOs::Linux, env).empty());
}

TEST_CASE("Editor command line: defaults and options", "[editor][cli]")
{
    CHECK(parse({}).ok());
    const auto r = parse({"--config-dir", "/tmp/cfg", "--reset-layout", "--frames", "60", "--screenshot", "a.png"});
    REQUIRE(r.ok());
    CHECK(r.options.configDirectory == "/tmp/cfg");
    CHECK(r.options.resetLayout);
    CHECK(r.options.maxFrames == 60);
    CHECK(r.options.screenshotPath == "a.png");
}

TEST_CASE("Editor command line: mistakes are reported", "[editor][cli]")
{
    CHECK(parse({"--frames", "0"}).error.find("--frames") != std::string::npos);
    CHECK(parse({"--screenshot", "a.png"}).error.find("--frames") != std::string::npos);
    CHECK(parse({"--config-dir"}).error == "--config-dir needs a value.");
    CHECK(parse({"--smoke-test"}).error == "Unknown option: --smoke-test");
    CHECK(parse({"--version"}).options.showVersion);
}
