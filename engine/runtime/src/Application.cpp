// Lantern runtime application — Axmol implementation.
//
// How the pieces fit together:
//
//   lantern::Application::run()
//     └─ creates BackendApp (an ax::Application) and calls its run()
//          └─ Axmol calls BackendApp::applicationDidFinishLaunching()
//               └─ Impl::startWindow(): window, diagnostics, the world
//                  (a SceneRenderer drawing a lantern::Scene),
//                  the optional UI layer (UiHost), the per-frame callback
//          └─ Axmol's main loop, every frame:
//               update → Impl::onFrame(): game callback, then the UI is
//                        updated, laid out and painted into a draw list
//               draw   → (Offscreen) WorldCaptureNode renders the world into
//                        a texture; then the UI's draw list is drawn on top
//          └─ loop ends when the window closes or Director::end() is called
//     └─ report statistics, restore logging, return a Status
//
// Axmol types never appear outside this folder.

#include "lantern/runtime/Application.hpp"

#include "AxmolLogBridge.hpp"
#include "SceneRenderer.hpp"
#include "UiHost.hpp"

#include <axmol.h>
#include <renderer/backend/DriverBase.h>

#include <algorithm>
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

// Renders the (hidden) world into a texture every frame. The world stays in
// the running scene so its animations and timers run normally; it is only
// made visible for the moment it is drawn into the texture.
class WorldCaptureNode final : public ax::Node {
public:
    static WorldCaptureNode* create(ax::Node* world, ax::RenderTexture* target)
    {
        auto* node = new WorldCaptureNode(world, target);
        node->autorelease();
        return node;
    }

    void visit(ax::Renderer* renderer, const ax::Mat4& /*parentTransform*/, uint32_t /*parentFlags*/) override
    {
        m_target->beginWithClear(0.0f, 0.0f, 0.0f, 1.0f);
        m_world->setVisible(true);
        m_world->visit(renderer, ax::Mat4::IDENTITY, ax::Node::FLAGS_TRANSFORM_DIRTY);
        m_world->setVisible(false);
        m_target->end();
    }

private:
    WorldCaptureNode(ax::Node* world, ax::RenderTexture* target)
        : m_world(world)
        , m_target(target)
    {
    }

    ax::Node* m_world;           // owned by the scene
    ax::RenderTexture* m_target; // owned by Application::Impl
};

} // namespace

// ---------------------------------------------------------------------------
// Impl: all state for one Application, including the Axmol-facing logic.
// ---------------------------------------------------------------------------
struct Application::Impl {
    Impl(Engine& e, WindowSettings w, PresentationSettings p)
        : engine(e)
        , window(std::move(w))
        , presentation(std::move(p))
    {
    }

    Engine& engine;
    WindowSettings window;
    PresentationSettings presentation;
    RunOptions options;
    std::function<void(const FrameInfo&)> frameCallback;
    RunReport report;

    bool hasRun = false;
    bool quitRequested = false;
    bool finishing = false;        // Director::end() has been requested
    bool screenshotFailed = false;
    std::string startupError;

    ax::Node* world = nullptr;                  // owned by the scene
    std::unique_ptr<detail::SceneRenderer> renderer;
    std::shared_ptr<const Scene> pendingScene;  // shown once the window exists
    const Scene* shownScene = nullptr;          // for logging scene changes
    ax::RenderTexture* worldTexture = nullptr;  // retained here (Offscreen only)
    std::unique_ptr<detail::UiHost> ui;
    float fps = 0.0f;


    Logger& log() { return engine.logger(); }
    bool uiEnabled() const { return static_cast<bool>(presentation.uiSetup); }

    bool startWindow();
    void startUi(ax::Scene& scene, ax::RenderView& view);
    void logDiagnostics(ax::RenderView& view);
    void onFrame(float deltaSeconds);
    void finish();
    void releaseBackendObjects();
    void showScene(std::shared_ptr<const Scene> scene)
    {
        if (renderer) {
            renderer->show(std::move(scene));
        } else {
            pendingScene = std::move(scene);
        }
    }
};

// UiFrame talks to the Impl through this small bridge.
struct UiFrame::Access {
    Application::Impl& impl;
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

    const auto worldW = static_cast<float>(presentation.worldWidth);
    const auto worldH = static_cast<float>(presentation.worldHeight);

    director->setRenderView(view);
    director->setStatsDisplay(options.showStats);
    director->setAnimationInterval(1.0f / 60.0f);
    // World coordinates = the game's resolution, whatever the window size.
    view->setDesignResolutionSize(worldW, worldH, ResolutionPolicy::SHOW_ALL);

    logDiagnostics(*view);

    auto* scene = ax::Scene::create();
    scene->setName("Root");
    // The world: an empty node the Lantern scene is drawn under.
    world = ax::Node::create();
    world->setName("World");
    world->setContentSize(ax::Size(worldW, worldH));
    scene->addChild(world);
    renderer = std::make_unique<detail::SceneRenderer>(*world);
    std::shared_ptr<const Scene> first = pendingScene != nullptr ? pendingScene : presentation.scene;
    if (first == nullptr) {
        first = std::make_shared<const Scene>(makeStartScene(worldW, worldH));
    }
    renderer->show(std::move(first));
    renderer->update();
    shownScene = renderer->scene().get();
    log().info(kCategory, "Showing scene '" + shownScene->name() + "' (" + std::to_string(shownScene->size())
                              + " entities)");

    if (presentation.worldDisplay == WorldDisplay::Offscreen) {
        worldTexture = ax::RenderTexture::create(presentation.worldWidth, presentation.worldHeight,
                                                 ax::backend::PixelFormat::RGBA8,
                                                 ax::backend::PixelFormat::D24S8);
        if (worldTexture == nullptr) {
            startupError = "Could not create the off-screen world image.";
            return false;
        }
        worldTexture->retain();
        world->setVisible(false); // drawn only by the capture node
        scene->addChild(WorldCaptureNode::create(world, worldTexture), -100);
        director->setClearColor(ax::Color4F(0.08f, 0.08f, 0.10f, 1.0f));
    }

    director->runWithScene(scene);

    if (options.escapeQuits) {
        auto* keyboard = ax::EventListenerKeyboard::create();
        keyboard->onKeyPressed = [this](ax::EventKeyboard::KeyCode code, ax::Event*) {
            if (code == ax::EventKeyboard::KeyCode::KEY_ESCAPE) {
                log().info(kCategory, "Escape pressed; closing.");
                quitRequested = true;
            }
        };
        director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(keyboard, scene);
    }

    if (uiEnabled()) {
        startUi(*scene, *view);
    }

    // The window's close button: let the application say "not yet" (e.g. to
    // ask about unsaved changes). Axmol asks through this event.
    director->getEventDispatcher()->addCustomEventListener(
        ax::RenderViewImpl::EVENT_WINDOW_CLOSE, [this](ax::EventCustom* event) {
            if (presentation.onCloseRequested && !finishing && !presentation.onCloseRequested()) {
                *static_cast<bool*>(event->getUserData()) = false;
            }
        });

    // Axmol announces EVENT_RESET while shutting down, while the window and
    // graphics context still exist: the right moment to release the UI layer
    // and the world texture (doing it after the window is gone would crash).
    director->getEventDispatcher()->addCustomEventListener(ax::Director::EVENT_RESET,
                                                           [this](ax::EventCustom*) { releaseBackendObjects(); });

    // Run onFrame() every frame (interval 0) for as long as the loop lives.
    director->getScheduler()->schedule([this](float dt) { onFrame(dt); }, this, 0.0f, false, "lantern.frame");
    return true;
}

void Application::Impl::startUi(ax::Scene& scene, ax::RenderView& view)
{
    auto* glfwWindow = static_cast<ax::RenderViewImpl&>(view).getWindow();
    ax::Texture2D* worldImage = worldTexture != nullptr ? worldTexture->getSprite()->getTexture() : nullptr;
    ui = std::make_unique<detail::UiHost>(log(), glfwWindow, scene, worldImage);
    ui->beginFrame(0.0);
    UiFrame::Access access{*this};
    UiFrame frame(access);
    presentation.uiSetup(frame);
    log().info(kCategory, "UI layer: Lantern UI");
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
    log().info(kCategory, "World: " + std::to_string(presentation.worldWidth) + "x"
                              + std::to_string(presentation.worldHeight) + " points, "
                              + (presentation.worldDisplay == WorldDisplay::Offscreen ? "off-screen" : "full window"));
}

void Application::Impl::onFrame(float deltaSeconds)
{
    if (frameCallback) {
        frameCallback(FrameInfo{report.framesRendered, static_cast<double>(deltaSeconds)});
    }
    ++report.framesRendered;

    // Smoothed frames per second (exponential moving average).
    if (deltaSeconds > 0.0f) {
        const float instant = 1.0f / deltaSeconds;
        fps = fps <= 0.0f ? instant : fps + (instant - fps) * 0.05f;
    }

    if (auto* view = ax::Director::getInstance()->getRenderView()) {
        const ax::Vec2 size = view->getFrameSize();
        report.finalWindowWidth = static_cast<int>(size.x);
        report.finalWindowHeight = static_cast<int>(size.y);
    }

    if (ui) {
        ui->beginFrame(static_cast<double>(deltaSeconds));
        if (presentation.uiUpdate) {
            UiFrame::Access access{*this};
            UiFrame frame(access);
            presentation.uiUpdate(frame);
        }
        ui->endFrame();
    }
    if (renderer) {
        const Scene* before = shownScene;
        renderer->update(); // redraw the scene if it changed this frame
        shownScene = renderer->scene().get();
        if (shownScene != before && shownScene != nullptr) {
            log().info(kCategory, "Showing scene '" + shownScene->name() + "' ("
                                      + std::to_string(shownScene->size()) + " entities)");
        }
    }

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

void Application::Impl::releaseBackendObjects()
{
    if (ui) {
        if (presentation.uiShutdown) {
            UiFrame::Access access{*this};
            UiFrame frame(access);
            presentation.uiShutdown(frame);
        }
        ui.reset(); // restores Axmol's input callbacks, frees UI textures
    }
    renderer.reset();
    AX_SAFE_RELEASE_NULL(worldTexture);
    world = nullptr;
}

// ---------------------------------------------------------------------------
// UiFrame
// ---------------------------------------------------------------------------
UiFrame::UiFrame(Access& access)
    : m_access(access)
{
}

ui::Context& UiFrame::ui()
{
    return m_access.impl.ui->context();
}

WorldImage UiFrame::worldImage() const
{
    const Application::Impl& impl = m_access.impl;
    if (impl.worldTexture == nullptr) {
        return {};
    }
    WorldImage image;
    image.texture = detail::UiHost::kWorldTexture;
    image.size = {static_cast<float>(impl.presentation.worldWidth), static_cast<float>(impl.presentation.worldHeight)};
#if AX_USE_GL
    // OpenGL stores render textures upside down.
    image.uv0 = {0.0f, 1.0f};
    image.uv1 = {1.0f, 0.0f};
#else
    image.uv0 = {0.0f, 0.0f};
    image.uv1 = {1.0f, 1.0f};
#endif
    return image;
}

int UiFrame::worldWidth() const noexcept
{
    return m_access.impl.presentation.worldWidth;
}

int UiFrame::worldHeight() const noexcept
{
    return m_access.impl.presentation.worldHeight;
}

float UiFrame::framesPerSecond() const noexcept
{
    return m_access.impl.fps;
}

void UiFrame::requestQuit()
{
    m_access.impl.quitRequested = true;
}

void UiFrame::showScene(std::shared_ptr<const Scene> scene)
{
    m_access.impl.showScene(std::move(scene));
}

void UiFrame::setWindowTitle(const std::string& title)
{
    if (auto* view = ax::Director::getInstance()->getRenderView()) {
        view->setViewName(title);
        if (auto* glfwView = dynamic_cast<ax::RenderViewImpl*>(view)) {
            glfwSetWindowTitle(glfwView->getWindow(), title.c_str());
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
Application::Application(Engine& engine, WindowSettings window, PresentationSettings presentation)
    : m_impl(std::make_unique<Impl>(engine, std::move(window), std::move(presentation)))
{
}

Application::~Application() = default;

void Application::showScene(std::shared_ptr<const Scene> scene)
{
    m_impl->showScene(std::move(scene));
}

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
    if (impl.presentation.worldWidth <= 0 || impl.presentation.worldHeight <= 0) {
        return Status::failure("World size must be positive.");
    }
    if (impl.presentation.worldDisplay == WorldDisplay::Offscreen && !impl.uiEnabled()) {
        return Status::failure("An off-screen world needs a UI layer to show it (PresentationSettings::uiSetup).");
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
        if (impl.ui || impl.worldTexture != nullptr) {
            // Normally released on EVENT_RESET (see startWindow). If the backend
            // stopped without announcing it, leaking is safer than touching a
            // destroyed window.
            impl.log().warning(kCategory, "Backend stopped without a reset event; UI resources not released.");
            static_cast<void>(impl.ui.release());
        }
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
