#include "lantern/assets/AssetDatabase.hpp"
#include "lantern/core/Files.hpp"
#include "lantern/scene/Scene.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace lantern;
namespace fs = std::filesystem;

namespace {

const fs::path kData = LANTERN_TEST_DATA_DIR;

// A fresh, empty project folder for each test.
struct TempProject {
    fs::path path;
    explicit TempProject(const char* name)
        : path(fs::temp_directory_path() / "lantern-tests" / name)
    {
        fs::remove_all(path);
        fs::create_directories(path / "scenes");
        std::ofstream(path / "project.lantern") << R"({"format": "lantern.project", "version": 1, "name": "T"})";
    }
    ~TempProject() { fs::remove_all(path); }
};

std::string readFile(const fs::path& p)
{
    return readTextFile(p).value_or("");
}

} // namespace

TEST_CASE("Asset types come from file extensions", "[assets]")
{
    CHECK(assetTypeForPath("a/hero.png") == AssetType::Image);
    CHECK(assetTypeForPath("HERO.JPG") == AssetType::Image);
    CHECK(assetTypeForPath("map.ltileset") == AssetType::Tileset);
    CHECK(assetTypeForPath("jump.ogg") == AssetType::Audio);
    CHECK(assetTypeForPath("ui.ttf") == AssetType::Font);
    CHECK_FALSE(assetTypeForPath("notes.txt").has_value());
    CHECK_FALSE(assetTypeForPath("hero.png.meta").has_value());
    CHECK(assetTypeFromName(assetTypeName(AssetType::Tileset)) == AssetType::Tileset);
}

TEST_CASE("Importing copies the file, writes a .meta with an ID and reads the image size", "[assets]")
{
    TempProject project("assets-import");
    AssetDatabase db(project.path);
    std::string error;
    const auto id = db.import(kData / "hero.png", "assets", &error);
    REQUIRE(id.has_value());
    CHECK(error.empty());
    CHECK(fs::exists(project.path / "assets/hero.png"));
    CHECK(fs::exists(project.path / "assets/hero.png.meta"));

    const AssetRecord* hero = db.find(*id);
    REQUIRE(hero != nullptr);
    CHECK(hero->path == "assets/hero.png");
    CHECK(hero->type == AssetType::Image);
    REQUIRE(hero->image.has_value());
    CHECK(hero->image->width == 24);
    CHECK(hero->image->height == 32);
    CHECK(hero->filter == TextureFilter::Pixel); // small images default to pixel art
    CHECK(hero->isUsable());
    CHECK(readFile(project.path / "assets/hero.png.meta").find(id->toString()) != std::string::npos);

    // A second import of a same-named file gets a new name and a new ID.
    const auto second = db.import(kData / "hero.png", "assets");
    REQUIRE(second.has_value());
    CHECK(*second != *id);
    CHECK(db.find(*second)->path == "assets/hero 2.png");

    // Large photos default to smooth filtering.
    const auto photo = db.import(kData / "photo.jpg", "assets");
    REQUIRE(photo.has_value());
    CHECK(db.find(*photo)->filter == TextureFilter::Smooth);
    CHECK(db.find(*photo)->image->width == 300);

    std::ofstream(project.path / "notes.txt") << "not an asset";
    CHECK_FALSE(db.import(project.path / "notes.txt", "assets", &error).has_value());
    CHECK(error.find("not a supported asset") != std::string::npos);

    // Scenes are assets too (so gameplay can refer to them by ID).
    const auto scene = db.import(kData / "test_card.lscene", "scenes");
    REQUIRE(scene.has_value());
    CHECK(db.find(*scene)->type == AssetType::Scene);
    CHECK(db.find(*scene)->isUsable());
    CHECK(assetTypeForPath("levels/cave.lscene") == AssetType::Scene);
    CHECK(assetTypeFromName("scene") == AssetType::Scene);
}

TEST_CASE("IDs survive a restart and a move; copies get new IDs", "[assets]")
{
    TempProject project("assets-ids");
    AssetId heroId;
    {
        AssetDatabase db(project.path);
        heroId = *db.import(kData / "hero.png", "assets");
        REQUIRE(db.move(heroId, "assets/characters/player.png"));
        CHECK(db.find(heroId)->path == "assets/characters/player.png");
        CHECK(fs::exists(project.path / "assets/characters/player.png.meta"));
        CHECK_FALSE(fs::exists(project.path / "assets/hero.png.meta"));
    }
    // A new database (as after restarting the editor) finds the same ID.
    AssetDatabase db(project.path);
    CHECK(db.refresh().empty());
    REQUIRE(db.find(heroId) != nullptr);
    CHECK(db.find(heroId)->path == "assets/characters/player.png");

    // The user copies the file *with* its .meta in Finder: the copy needs its own ID.
    fs::copy_file(project.path / "assets/characters/player.png", project.path / "assets/copy.png");
    fs::copy_file(project.path / "assets/characters/player.png.meta", project.path / "assets/copy.png.meta");
    const auto issues = db.refresh();
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].message.find("same ID") != std::string::npos);
    const AssetRecord* copy = db.findByPath("assets/copy.png");
    REQUIRE(copy != nullptr);
    CHECK(copy->id != heroId);
    CHECK(db.find(heroId) != nullptr);
}

TEST_CASE("Files added by hand get a .meta; damaged and missing files are reported once", "[assets]")
{
    TempProject project("assets-scan");
    fs::create_directories(project.path / "assets");
    fs::copy_file(kData / "tiles_16px.png", project.path / "assets/tiles.png");
    fs::copy_file(kData / "broken.png", project.path / "assets/broken.png");
    std::ofstream(project.path / "assets/readme.txt") << "not an asset";

    AssetDatabase db(project.path);
    const auto issues = db.refresh();
    CHECK(fs::exists(project.path / "assets/tiles.png.meta"));
    CHECK(db.all().size() == 2);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].where == "assets/broken.png");
    CHECK(issues[0].message.find("not a readable PNG or JPEG") != std::string::npos);
    CHECK_FALSE(db.findByPath("assets/broken.png")->isUsable());

    // Nothing changed: nothing new to report, and the revision stays.
    const auto revision = db.revision();
    CHECK(db.refresh().empty());
    CHECK(db.revision() == revision);

    // The image is deleted outside the editor, leaving its .meta behind.
    fs::remove(project.path / "assets/tiles.png");
    const auto gone = db.refresh();
    REQUIRE(gone.size() == 1);
    CHECK(gone[0].message.find("missing") != std::string::npos);
    CHECK(db.findByPath("assets/tiles.png") == nullptr);
    CHECK(db.revision() > revision);
}

TEST_CASE("A filter change is saved in the .meta and kept after a restart", "[assets]")
{
    TempProject project("assets-filter");
    AssetId id;
    {
        AssetDatabase db(project.path);
        id = *db.import(kData / "hero.png");
        REQUIRE(db.setFilter(id, TextureFilter::Smooth));
    }
    AssetDatabase db(project.path);
    db.refresh();
    CHECK(db.find(id)->filter == TextureFilter::Smooth);
}

TEST_CASE("Tilesets: geometry, collision, files", "[assets][tileset]")
{
    Tileset t;
    t.tileWidth = 16;
    t.tileHeight = 16;
    CHECK(t.columns(64) == 4);
    CHECK(t.rows(32) == 2);
    const TileRect r = t.tileRect(5, 64);
    CHECK(r.x == 16);
    CHECK(r.y == 16);

    t.margin = 1;
    t.spacing = 2;
    CHECK(t.columns(1 + 16 + 2 + 16 + 1) == 2);
    CHECK(t.tileRect(1, 36).x == 19);

    t.image = AssetId(0x1234);
    t.solid = {3, 7};
    std::string error;
    const auto back = parseTileset(serializeTileset(t), &error);
    REQUIRE(back.has_value());
    CHECK(*back == t);
    CHECK(back->isSolid(7));
    CHECK_FALSE(back->isSolid(2));

    CHECK_FALSE(parseTileset("{}", &error).has_value());
    CHECK_FALSE(parseTileset(R"({"format": "lantern.tileset", "version": 1, "tileSize": [0, 16]})", &error));
    CHECK(error.find("between 1 and 4096") != std::string::npos);
    CHECK_FALSE(parseTileset(R"({"format": "lantern.tileset", "version": 9, "tileSize": [16, 16]})", &error));
    CHECK(error.find("newer version") != std::string::npos);
}

TEST_CASE("Creating and saving a tileset asset", "[assets][tileset]")
{
    TempProject project("assets-tileset");
    AssetDatabase db(project.path);
    const AssetId image = *db.import(kData / "tiles_16px.png");
    std::string error;
    const auto tileset = db.createTileset(image, {16, 16}, "Grass", &error);
    REQUIRE(tileset.has_value());
    CHECK(db.find(*tileset)->path == "assets/Grass.ltileset");
    REQUIRE(db.find(*tileset)->tileset.has_value());

    Tileset changed = *db.find(*tileset)->tileset;
    changed.solid.insert(6);
    REQUIRE(db.saveTileset(*tileset, changed));

    AssetDatabase reopened(project.path);
    CHECK(reopened.refresh().empty());
    REQUIRE(reopened.find(*tileset) != nullptr);
    CHECK(reopened.find(*tileset)->tileset->isSolid(6));
    CHECK(reopened.find(*tileset)->tileset->image == image);
}

TEST_CASE("Scenes are checked for missing assets and found by reference", "[assets]")
{
    TempProject project("assets-refs");
    AssetDatabase db(project.path);
    const AssetId hero = *db.import(kData / "hero.png");

    // A scene file that mentions the asset's ID.
    std::ofstream(project.path / "scenes/level.lscene") << "{ \"image\": \"" + hero.toString() + "\" }";
    std::ofstream(project.path / "scenes/other.lscene") << "{ }";
    const auto users = db.scenesReferencing(hero);
    REQUIRE(users.size() == 1);
    CHECK(users[0] == "scenes/level.lscene");

    // A scene with no asset properties has nothing to report.
    const Scene scene = makeStartScene();
    CHECK(checkAssetReferences(scene, db).empty());
}

TEST_CASE("Small images copied in by hand also default to pixel-art filtering", "[assets]")
{
    TempProject project("assets-default-filter");
    fs::create_directories(project.path / "assets");
    fs::copy_file(kData / "hero.png", project.path / "assets/hero.png");
    fs::copy_file(kData / "photo.jpg", project.path / "assets/photo.jpg");
    AssetDatabase db(project.path);
    db.refresh();
    CHECK(db.findByPath("assets/hero.png")->filter == TextureFilter::Pixel);
    CHECK(db.findByPath("assets/photo.jpg")->filter == TextureFilter::Smooth);
}

TEST_CASE("A read-only database (games) never writes .meta files", "[assets]")
{
    TempProject project("assets-readonly");
    fs::create_directories(project.path / "assets");
    fs::copy_file(kData / "hero.png", project.path / "assets/hero.png");
    AssetDatabase db(project.path, AssetDatabase::Access::ReadOnly);
    CHECK(db.refresh().empty());
    REQUIRE(db.findByPath("assets/hero.png") != nullptr); // usable for this run
    CHECK_FALSE(fs::exists(project.path / "assets/hero.png.meta"));
}
