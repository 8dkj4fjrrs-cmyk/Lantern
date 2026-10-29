#pragma once

// Lantern runtime application: opens the game window and runs the main loop.
//
// This header deliberately contains no Axmol (or other backend) types, and no
// Dear ImGui types either. Game code and tools talk to Lantern; Lantern talks
// to the backend in src/.

#include "lantern/core/Engine.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lantern {

/// How the game window should look when it opens.
struct WindowSettings {
    std::string title = "Lantern Engine";
    int width = 1280;  // in screen points (a Retina screen draws 2x the pixels)
    int height = 720;
    bool resizable = true;
};

/// Where the game world is drawn.
enum class WorldDisplay {
    /// The world fills the window (letterboxed to keep its shape). Used by games.
    FullWindow,
    /// The world is drawn into an off-screen image that the immediate-mode UI
    /// shows with UiFrame::worldImage(). Used by the editor's viewport.
    Offscreen,
};

/// How the runtime presents the world and the optional immediate-mode UI.
struct PresentationSettings {
    WorldDisplay worldDisplay = WorldDisplay::FullWindow;

    /// The game's own resolution in points. World coordinates run from (0, 0)
    /// at the bottom-left to (worldWidth, worldHeight) at the top-right.
    int worldWidth = 1280;
    int worldHeight = 720;

    /// Turn on the immediate-mode UI layer (Dear ImGui) and call this every
    /// frame to build it. Empty = no UI layer.
    std::function<void(class UiFrame&)> ui;

    /// Called once when the UI layer has started, before its first frame
    /// (e.g. to load a saved layout or set colours). Optional.
    std::function<void()> uiSetup;

    /// Called once just before the UI layer shuts down, while it still exists
    /// (e.g. to save the layout). Optional.
    std::function<void()> uiShutdown;

    /// Optional TrueType font file for the UI, and its size in points.
    /// Empty or missing file = Dear ImGui's built-in font.
    std::filesystem::path uiFontFile;
    float uiFontSize = 15.0f;
};

/// Optional behaviour for a single run, mostly useful for automated tests.
struct RunOptions {
    /// Stop automatically after this many frames. 0 = run until the window is closed.
    /// (The backend may draw a few extra frames while shutting down or saving
    /// a screenshot; RunReport counts every frame actually drawn.)
    std::uint64_t maxFrames = 0;

    /// If set, save a PNG of the final frame here before stopping.
    /// Requires maxFrames > 0 so there *is* a final frame.
    std::filesystem::path screenshotPath;

    /// Show the backend's frame-rate / draw-call overlay in the corner.
    bool showStats = true;

    /// Close the window when Escape is pressed (games: yes; editor: no).
    bool escapeQuits = true;
};

/// Passed to the per-frame callback.
struct FrameInfo {
    std::uint64_t frameIndex = 0;  // 0 for the first frame
    double deltaSeconds = 0.0;     // time since the previous frame
};

/// Summary returned after the window closes.
struct RunReport {
    std::uint64_t framesRendered = 0;
    double elapsedSeconds = 0.0;
    int finalWindowWidth = 0;   // window size (points) when the run ended
    int finalWindowHeight = 0;
    double averageFps() const { return elapsedSeconds > 0.0 ? static_cast<double>(framesRendered) / elapsedSeconds : 0.0; }
};

/// A read-only snapshot of one object in the running scene, for tools such as
/// the editor's hierarchy and inspector panels.
struct SceneNodeInfo {
    std::uint64_t id = 0;        // unique while the application runs (not saved to disk)
    std::uint64_t parentId = 0;  // 0 = top level
    int depth = 0;               // 0 = top level
    std::string name;            // empty if the object has no name
    std::string kind;            // "Node", "Shape", "Sprite", "Text", ...
    float x = 0.0f;              // position relative to the parent, in points
    float y = 0.0f;
    float rotationDegrees = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float width = 0.0f;          // content size, in points (0 for pure containers)
    float height = 0.0f;
    int drawOrder = 0;           // higher draws on top of siblings
    bool visible = true;
    std::size_t childCount = 0;
};

/// Where the world image was placed by UiFrame::worldImage(), in UI points.
struct WorldImageRect {
    float x = 0.0f;       // top-left corner, screen coordinates
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float scale = 0.0f;   // displayed size / world size
};

/// Runtime services available while building the UI for one frame.
/// Only valid inside the PresentationSettings::ui callback.
class UiFrame {
public:
    /// Draws the off-screen world image at the current UI cursor, scaled to fit
    /// inside (availableWidth x availableHeight) without changing its shape,
    /// centred. Only works with WorldDisplay::Offscreen; otherwise draws nothing.
    WorldImageRect worldImage(float availableWidth, float availableHeight);

    /// The world's resolution in points.
    int worldWidth() const noexcept;
    int worldHeight() const noexcept;

    /// A snapshot of every object in the game world, parents before children.
    const std::vector<SceneNodeInfo>& sceneOutline();

    /// UI frames per second, averaged over the last second.
    float framesPerSecond() const noexcept;

    /// Asks the main loop to finish (same as Application::requestQuit()).
    void requestQuit();

    struct Access; // implementation detail
    explicit UiFrame(Access& access);

private:
    Access& m_access;
};

/// Owns the window and main loop for one run of the game.
///
/// Typical use:
///     lantern::Engine engine({.applicationName = "My Game"});
///     engine.initialize();
///     lantern::Application app(engine, {.title = "My Game"});
///     auto status = app.run();
///     engine.shutdown();
///
/// Only one Application may run per process (desktop platforms have one
/// main window and one graphics context), and run() may be called only once.
class Application {
public:
    Application(Engine& engine, WindowSettings window, PresentationSettings presentation = {});
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    /// Opens the window and blocks until it closes (user closes it, Escape is
    /// pressed, requestQuit() is called, or RunOptions::maxFrames is reached).
    Status run(const RunOptions& options = {});

    /// Asks the main loop to finish after the current frame. Safe to call from
    /// the per-frame or UI callback.
    void requestQuit();

    /// Called once per frame before drawing. Game logic will hook in here.
    void setFrameCallback(std::function<void(const FrameInfo&)> callback);

    /// Statistics from the last completed run().
    const RunReport& lastRunReport() const noexcept;

    /// Implementation details (hidden so that no backend types leak out).
    struct Impl;

private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace lantern
