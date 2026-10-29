#pragma once

// Lantern runtime application: opens the game window and runs the main loop.
//
// This header deliberately contains no Axmol (or other backend) types. Game
// code and tools talk to Lantern; Lantern talks to the backend in src/.

#include "lantern/core/Engine.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace lantern {

/// How the game window should look when it opens.
struct WindowSettings {
    std::string title = "Lantern Engine";
    int width = 1280;  // in screen points (a Retina screen draws 2x the pixels)
    int height = 720;
    bool resizable = true;
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
    double averageFps() const { return elapsedSeconds > 0.0 ? static_cast<double>(framesRendered) / elapsedSeconds : 0.0; }
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
    Application(Engine& engine, WindowSettings window);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    /// Opens the window and blocks until it closes (user closes it, Escape is
    /// pressed, requestQuit() is called, or RunOptions::maxFrames is reached).
    Status run(const RunOptions& options = {});

    /// Asks the main loop to finish after the current frame. Safe to call from
    /// the per-frame callback.
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
