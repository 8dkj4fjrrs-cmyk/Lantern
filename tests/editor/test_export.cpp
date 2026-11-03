// Exporting a project as a stand-alone game (editor/model/GameExport.hpp),
// with a stand-in runtime for each operating system's layout.

#include "support/TempFolder.hpp"
#include "GameExport.hpp"

#include "lantern/core/Files.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace lantern;
using namespace lantern::editor;
namespace fs = std::filesystem;

namespace {

struct Setup {
    fs::path root = test::uniqueTempFolder("export");
    ExportOptions options;

    Setup()
    {
        file("project/project.lantern", R"({"format": "lantern.project", "version": 1, "name": "My Game"})");
        file("project/scenes/main.lscene", "{}");
        file("project/assets/hero.png", "png");
        file("project/.git/config", "not for players");
        file("licenses/README.md", "# Licenses");
        file("licenses/axmol.txt", "MIT");
        options.project = root / "project";
        options.output = root / "exports";
        options.gameName = "My Game!";
        options.licenses = root / "licenses";
        options.version = "0.8.0";
    }
    ~Setup()
    {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    void file(const fs::path& rel, const std::string& text = "x")
    {
        fs::create_directories((root / rel).parent_path());
        std::ofstream(root / rel) << text;
    }
};

} // namespace

TEST_CASE("Export names are safe file names", "[editor][export]")
{
    CHECK(exportFileName("My Game!") == "My Game");
    CHECK(exportFileName("  a/b:c  ") == "abc");
    CHECK(exportFileName("???") == "Game");
    CHECK(exportFileName("Café_2-B") == "Café_2-B");
}

TEST_CASE("A Linux or Windows export is the runtime, renamed, with the game beside it", "[editor][export]")
{
    for (const HostOs os : {HostOs::Linux, HostOs::Windows}) {
        Setup s;
        const std::string exe = os == HostOs::Windows ? "lantern_player.exe" : "lantern_player";
        s.file("runtime/" + exe, "program");
        s.file("runtime/axslc/positionColor_vs", "shader");
        s.file("runtime/libGLESv2.dll", "dll");
        s.file("runtime/lantern_player.pdb", "debug info");
        s.options.player = s.root / "runtime" / exe;
        s.options.os = os;
        const ExportResult r = exportGame(s.options);
        INFO(r.error);
        REQUIRE(r.ok);
        const fs::path game = s.root / "exports/My Game";
        CHECK(r.game == game);
        CHECK(r.program == game / (os == HostOs::Windows ? "My Game.exe" : "My Game"));
        CHECK(*readTextFile(r.program) == "program");
        CHECK(fs::exists(game / "axslc/positionColor_vs"));
        CHECK(fs::exists(game / "libGLESv2.dll"));
        CHECK_FALSE(fs::exists(game / "lantern_player.pdb"));   // build leftovers stay behind
        CHECK(fs::exists(game / "game/project.lantern"));
        CHECK(fs::exists(game / "game/assets/hero.png"));
        CHECK_FALSE(fs::exists(game / "game/.git"));               // hidden files stay behind
        CHECK(fs::exists(game / "licenses/axmol.txt"));
        CHECK(readTextFile(game / "README.txt")->find("Lantern Engine 0.8.0") != std::string::npos);
        CHECK(r.files >= 7);
        if (os == HostOs::Linux) {
            CHECK((fs::status(r.program).permissions() & fs::perms::owner_exec) != fs::perms::none);
        }

        // Exporting again replaces the earlier export.
        s.file("project/scenes/second.lscene", "{}");
        const ExportResult again = exportGame(s.options);
        REQUIRE(again.ok);
        CHECK(fs::exists(game / "game/scenes/second.lscene"));
    }
}

TEST_CASE("A macOS export is the runtime app, renamed, with the game inside", "[editor][export]")
{
    Setup s;
    s.file("runtime/lantern_player.app/Contents/MacOS/lantern_player", "program");
    s.file("runtime/lantern_player.app/Contents/Resources/axslc/positionColor_vs", "shader");
    s.file("runtime/lantern_player.app/Contents/Info.plist",
           "<dict>\n\t<key>CFBundleIdentifier</key>\n\t<string>dev.lantern-engine.player</string>\n"
           "\t<key>CFBundleName</key>\n\t<string>Lantern Player</string>\n</dict>\n");
    s.options.player = s.root / "runtime/lantern_player.app";
    s.options.os = HostOs::MacOS;
    const ExportResult r = exportGame(s.options);
    INFO(r.error);
    REQUIRE(r.ok);
    const fs::path app = s.root / "exports/My Game.app";
    CHECK(r.game == app);
    CHECK(fs::exists(app / "Contents/MacOS/lantern_player"));
    CHECK(fs::exists(app / "Contents/Resources/axslc/positionColor_vs"));
    CHECK(fs::exists(app / "Contents/Resources/game/scenes/main.lscene"));
    CHECK(fs::exists(app / "Contents/Resources/licenses/axmol.txt"));
    const std::string plist = *readTextFile(app / "Contents/Info.plist");
    CHECK(plist.find("<string>My Game</string>") != std::string::npos);
    CHECK(plist.find("dev.lantern-engine.game.mygame") != std::string::npos);
}

TEST_CASE("Exports refuse what they cannot do safely, saying why", "[editor][export]")
{
    Setup s;
    s.file("runtime/lantern_player", "program");
    s.options.player = s.root / "runtime/lantern_player";
    s.options.os = HostOs::Linux;

    ExportOptions noRuntime = s.options;
    noRuntime.player = s.root / "missing/lantern_player";
    CHECK(exportGame(noRuntime).error.find("runtime") != std::string::npos);

    ExportOptions inside = s.options;
    inside.output = s.root / "project/exports";
    CHECK(exportGame(inside).error.find("outside the project") != std::string::npos);

    ExportOptions noLicenses = s.options;
    noLicenses.licenses = s.root / "nowhere";
    CHECK(exportGame(noLicenses).error.find("license") != std::string::npos);

    // A folder of the same name that is not an earlier export is never deleted.
    s.file("exports/My Game/precious.txt", "keep me");
    const ExportResult r = exportGame(s.options);
    CHECK_FALSE(r.ok);
    CHECK(r.error.find("not an earlier export") != std::string::npos);
    CHECK(fs::exists(s.root / "exports/My Game/precious.txt"));
}
