// Lantern runtime application — Axmol implementation.
//
// How the pieces fit together:
//
//   lantern::Application::run()
//     └─ creates BackendApp (an ax::Application) and calls its run()
//          └─ Axmol calls BackendApp::applicationDidFinishLaunching()
//               └─ Impl::startWindow(): create window, log diagnostics,
//                  show the start scene, register the per-frame callback
//          └─ Axmol's main loop: every frame → Impl::onFrame() → draw
//          └─ loop ends when the window closes or Director::end() is called
//     └─ report statistics, restore logging, return a Status
//
// Axmol types never appear outside this folder.

#include "lantern/runtime/Application.hpp"

#include "AxmolLogBridge.hpp"
#include "StartScene.hpp"

#include <axmol.h>
#include <renderer/backend/DriverBase.h>

#include <atomic>
#include <chrono>
#include <string>
#include <utility>

namespace lantern {

namespace {

constexpr std::string_view kCategory = "runtime";

// Desktop platforms have exactly one main window and graphics context, and
// Axmol's Application/Director are process-wide singletons. This flag turns
// an accidental second Application into a clear error instead of a crash.
// (A deliberate, documented exception to "no global mutable state".)
std::atomic<bool> g_backendInUse{false};

std::string toString(ax::Vec2 size)
{
    return std::to_string(static_cast<int>(size.x)) + "x" + std::to_string(static_cast<int>(size.y));
}

} // namespace

// ---------------------------------------------------------------------------
// Impl: all state for one Application, including the Axmol-facing logic.
// ---------------------------------------------------------------------------
struct Application::Impl {
    Impl(Engine& e, WindowSettings w)
        : engine(e)
        , window(std::move(w))
    {
    }

    Engine& engine;
    WindowSettings window;
    RunOptions options;
    std::function<void(const FrameInfo&)> frameCallback;
    RunReport report;

    bool hasRun = false;
    bool quitRequested = false;
    bool finishing = false;        // Director::end() has been requested
    bool screenshotFailed = false;
    std::string startupError;

    Logger& log() { return engine.logger(); }

    bool startWindow();
    void logDiagnostics(ax::RenderView& view);
    void onFrame(float deltaSeconds);
    void finish();
};

namespace {

// Axmol drives the program through an ax::Application subclass. This one
// simply forwards the interesting moments to Application::Impl.
class BackendApp final : public ax::Application {
public:
    explicit BackendApp(lantern::Application::Impl& impl)
        : m_impl(impl)
    {
    }

    void initGfxContextAttrs() override
    {
        // 8 bits per colour channel, 24-bit depth, 8-bit stencil, no MSAA.
        ::GfxContextAttrs attrs{}; // Axmol declares this outside namespace ax
        attrs.redBits = 8;
        attrs.greenBits = 8;
        attrs.blueBits = 8;
        attrs.alphaBits = 8;
        attrs.depthBits = 24;
        attrs.stencilBits = 8;
        attrs.multisamplingCount = 0;
        ax::RenderView::setGfxContextAttrs(attrs);
    }

    bool applicationDidFinishLaunching() override { return m_impl.startWindow(); }

    // Pause the loop while the app is minimised / in the background.
    void applicationDidEnterBackground() override { ax::Director::getInstance()->stopAnimation(); }
    void applicationWillEnterForeground() override { ax::Director::getInstance()->startAnimation(); }

private:
    lantern::Application::Impl& m_impl;
};

} // namespace

bool Application::Impl::startWindow()
{
    ax::Director* director = ax::Director::getInstance();

    ax::RenderView* view = ax::RenderViewImpl::createWithRect(
        window.title,
        ax::Rect(0, 0, static_cast<float>(window.width), static_cast<float>(window.height)),
        1.0f,
        window.resizable);
    if (view == nullptr) {
        startupError = "Could not create the game window. Check that your graphics drivers are up to date.";
        return false;
    }

    director->setRenderView(view);
    director->setStatsDisplay(options.showStats);
    director->setAnimationInterval(1.0f / 60.0f);
    view->setDesignResolutionSize(static_cast<float>(window.width), static_cast<float>(window.height),
                                  ResolutionPolicy::SHOW_ALL);

    logDiagnostics(*view);

    ax::Scene* scene = detail::createStartScene();
    director->runWithScene(scene);

    // Escape closes the window — handy while there is no menu yet.
    auto* keyboard = ax::EventListenerKeyboard::create();
    keyboard->onKeyPressed = [this](ax::EventKeyboard::KeyCode code, ax::Event*) {
        if (code == ax::EventKeyboard::KeyCode::KEY_ESCAPE) {
            log().info(kCategory, "Escape pressed; closing.");
            quitRequested = true;
        }
    };
    director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(keyboard, scene);

    // Run onFrame() every frame (interval 0) for as long as the loop lives.
    director->getScheduler()->schedule([this](float dt) { onFrame(dt); }, this, 0.0f, false, "lantern.frame");
    return true;
}

void Application::Impl::logDiagnostics(ax::RenderView& view)
{
#if AX_USE_METAL
    const std::string_view graphicsApi = "Metal";
#else
    const std::string_view graphicsApi = "OpenGL";
#endif
    auto* driver = ax::backend::DriverBase::getInstance();

    log().info(kCategory, std::string("Backend: ") + ax::axmolVersion());
    log().info(kCategory, "Graphics API: " + std::string(graphicsApi));
    if (driver != nullptr) {
        log().info(kCategory, std::string("GPU: ") + driver->getRenderer() + " (" + driver->getVendor() + ")");
        log().info(kCategory, std::string("Driver version: ") + driver->getVersion());
    }
    log().info(kCategory, "Window: '" + window.title + "', " + toString(view.getFrameSize())
                              + " points, retina factor " + std::to_string(view.getRetinaFactor()));
}

void Application::Impl::onFrame(float deltaSeconds)
{
    if (frameCallback) {
        frameCallback(FrameInfo{report.framesRendered, static_cast<double>(deltaSeconds)});
    }
    ++report.framesRendered;

    if (finishing) {
        return;
    }

    const bool frameLimitReached = options.maxFrames > 0 && report.framesRendered >= options.maxFrames;
    if (quitRequested) {
        finish();
    } else if (frameLimitReached) {
        if (options.screenshotPath.empty()) {
            finish();
            return;
        }
        // Capture what is on screen, then stop. Axmol saves the image after
        // the frame is drawn and calls us back with the result.
        finishing = true;
        const std::string path = options.screenshotPath.string();
        ax::utils::captureScreen(
            [this, path](bool ok, std::string_view savedTo) {
                if (ok) {
                    log().info(kCategory, "Screenshot saved: " + std::string(savedTo));
                } else {
                    screenshotFailed = true;
                    log().error(kCategory, "Screenshot could not be saved to " + path);
                }
                ax::Director::getInstance()->end();
            },
            path);
    }
}

void Application::Impl::finish()
{
    finishing = true;
    ax::Director::getInstance()->end(); // the loop exits after this frame
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
Application::Application(Engine& engine, WindowSettings window)
    : m_impl(std::make_unique<Impl>(engine, std::move(window)))
{
}

Application::~Application() = default;

void Application::requestQuit()
{
    m_impl->quitRequested = true;
}

void Application::setFrameCallback(std::function<void(const FrameInfo&)> callback)
{
    m_impl->frameCallback = std::move(callback);
}

const RunReport& Application::lastRunReport() const noexcept
{
    return m_impl->report;
}

Status Application::run(const RunOptions& options)
{
    Impl& impl = *m_impl;

    // --- Check everything we can before touching the backend. -------------
    if (impl.hasRun) {
        return Status::failure("Application::run() can only be called once.");
    }
    if (!impl.engine.isRunning()) {
        return Status::failure("Call Engine::initialize() before Application::run().");
    }
    if (impl.window.width <= 0 || impl.window.height <= 0) {
        return Status::failure("Window size must be positive, got " + std::to_string(impl.window.width) + "x"
                               + std::to_string(impl.window.height) + ".");
    }
    if (!options.screenshotPath.empty() && options.maxFrames == 0) {
        return Status::failure("A screenshot needs a frame limit (RunOptions::maxFrames > 0).");
    }
    if (g_backendInUse.exchange(true)) {
        return Status::failure("Only one Lantern Application can run at a time in a process.");
    }

    impl.hasRun = true;
    impl.options = options;
    impl.report = {};

    // --- Run. ----------------------------------------------------------------
    int exitCode = 0;
    const auto start = std::chrono::steady_clock::now();
    {
        detail::AxmolLogBridge logBridge(impl.engine.logger()); // backend logs → our logger
        impl.log().info(kCategory, "Opening window...");
        BackendApp backend(impl);
        exitCode = backend.run(); // blocks until the window closes
    }
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    impl.report.elapsedSeconds = elapsed.count();
    g_backendInUse = false;

    // --- Report. -------------------------------------------------------------
    impl.log().info(kCategory, "Window closed after " + std::to_string(impl.report.framesRendered) + " frames ("
                                   + std::to_string(static_cast<int>(impl.report.averageFps() + 0.5))
                                   + " fps average).");

    if (!impl.startupError.empty()) {
        return Status::failure(impl.startupError);
    }
    if (exitCode != 0) {
        return Status::failure("The runtime backend stopped with error code " + std::to_string(exitCode) + ".");
    }
    if (impl.screenshotFailed) {
        return Status::failure("The screenshot could not be saved.");
    }
    return Status::success();
}

} // namespace lantern
