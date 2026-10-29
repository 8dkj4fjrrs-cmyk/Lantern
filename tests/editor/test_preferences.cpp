#include "EditorState.hpp"
#include "Preferences.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using lantern::editor::EditorState;
using lantern::editor::Preferences;

namespace {

// A fresh, empty folder under the system temp directory, removed afterwards.
struct TempDir {
    std::filesystem::path path;
    explicit TempDir(const char* name)
        : path(std::filesystem::temp_directory_path() / "lantern-tests" / name)
    {
        std::filesystem::remove_all(path);
    }
    ~TempDir() { std::filesystem::remove_all(path); }
};

} // namespace

TEST_CASE("Typed values round-trip through text", "[editor][prefs]")
{
    Preferences prefs;
    prefs.setBool("panel.console", false);
    prefs.setInt("window.width", 1440);
    prefs.setString("theme.name", "Lantern Dark");

    Preferences reloaded;
    CHECK(reloaded.parse(prefs.serialize()) == 0);
    CHECK_FALSE(reloaded.getBool("panel.console", true));
    CHECK(reloaded.getInt("window.width", 0) == 1440);
    CHECK(reloaded.getString("theme.name", "") == "Lantern Dark");
}

TEST_CASE("Missing or unreadable values fall back to defaults", "[editor][prefs]")
{
    Preferences prefs;
    prefs.parse("window.width = wide\npanel.console = maybe\n");
    CHECK(prefs.getInt("window.width", 1600) == 1600);
    CHECK(prefs.getBool("panel.console", true));
    CHECK(prefs.getInt("not.there", 7) == 7);
    CHECK_FALSE(prefs.getString("not.there").has_value());
}

TEST_CASE("Comments, blank lines and spaces are tolerated; bad lines are counted", "[editor][prefs]")
{
    Preferences prefs;
    const int skipped = prefs.parse("# comment\n\n   window.height=800  \nno equals sign\nbad key! = 1\r\n");
    CHECK(skipped == 2);
    CHECK(prefs.getInt("window.height", 0) == 800);
}

TEST_CASE("Invalid keys are ignored and line breaks cannot break the file", "[editor][prefs]")
{
    Preferences prefs;
    prefs.setString("has space", "x");
    CHECK(prefs.size() == 0);

    prefs.setString("note", "line one\nline two");
    Preferences reloaded;
    reloaded.parse(prefs.serialize());
    CHECK(reloaded.getString("note", "") == "line one line two");
}

TEST_CASE("Unknown keys survive a load/save cycle", "[editor][prefs]")
{
    Preferences prefs;
    prefs.parse("future.setting = 42\n");
    EditorState state;
    state.loadFrom(prefs);
    state.saveTo(prefs);
    CHECK(prefs.getInt("future.setting", 0) == 42);
}

TEST_CASE("Saving and loading a file, including creating its folder", "[editor][prefs]")
{
    TempDir dir("prefs-save");
    const auto file = dir.path / "nested" / "editor.prefs";

    Preferences prefs;
    prefs.setInt("window.width", 1234);
    REQUIRE(prefs.save(file));
    CHECK(std::filesystem::exists(file));
    CHECK_FALSE(std::filesystem::exists(file.string() + ".tmp")); // no leftovers

    Preferences loaded;
    REQUIRE(loaded.load(file));
    CHECK(loaded.getInt("window.width", 0) == 1234);
}

TEST_CASE("A missing preferences file is not an error", "[editor][prefs]")
{
    TempDir dir("prefs-missing");
    Preferences prefs;
    CHECK(prefs.load(dir.path / "does-not-exist.prefs"));
    CHECK(prefs.size() == 0);
}

TEST_CASE("Editor state is remembered: panels and window size", "[editor][state]")
{
    EditorState original;
    original.panels.console = false;
    original.panels.inspector = false;
    original.setWindowSize(1280, 800);
    original.selectedNodeId = 99; // session-only, must not be saved

    Preferences prefs;
    original.saveTo(prefs);

    EditorState restored;
    restored.loadFrom(prefs);
    CHECK_FALSE(restored.panels.console);
    CHECK_FALSE(restored.panels.inspector);
    CHECK(restored.panels.viewport);
    CHECK(restored.windowWidth == 1280);
    CHECK(restored.windowHeight == 800);
    CHECK(restored.selectedNodeId == 0);
}

TEST_CASE("Window sizes are clamped and unknown sizes ignored", "[editor][state]")
{
    EditorState state;
    state.setWindowSize(100, 999999);
    CHECK(state.windowWidth == EditorState::kMinWindowSize);
    CHECK(state.windowHeight == EditorState::kMaxWindowSize);

    state.setWindowSize(0, 0); // e.g. the window never opened
    CHECK(state.windowWidth == EditorState::kMinWindowSize);
}
