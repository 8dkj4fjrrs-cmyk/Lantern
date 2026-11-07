#pragma once

// UiHost: connects a rolo::ui::Context to the real window.
//
//   input    GLFW callbacks (keys, typed text, mouse, scroll) → Context events.
//            Axmol's own GLFW callbacks are kept and still called first.
//   output   the Context's DrawList → Axmol render commands (triangles,
//            textures, clipping), drawn on top of the scene.
//   extras   clipboard, mouse cursor shapes, the glyph-atlas texture.
//
// Internal to rolo_runtime.

#include "rolo/core/Log.hpp"
#include "rolo/ui/Context.hpp"

#include <axmol.h>                 // must come first: it sets up the OpenGL loader
#include <platform/RenderViewImpl.h> // brings in GLFW the way Axmol expects
#include <renderer/backend/VertexLayout.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace rolo::detail {

class UiRenderNode;

class UiHost {
public:
    /// Texture id of the off-screen world image (when there is one).
    static constexpr ui::TextureId kWorldTexture = 2;

    /// `worldTexture` may be null (no off-screen world).
    UiHost(const Logger& logger, GLFWwindow* window, ax::Scene& scene, ax::Texture2D* worldTexture);
    ~UiHost();

    UiHost(const UiHost&) = delete;
    UiHost& operator=(const UiHost&) = delete;

    ui::Context& context() noexcept { return *m_context; }
    /// Framebuffer pixels per window point (2 on a Retina screen).
    float pixelScale() const noexcept { return m_pixelScale; }

    /// Replaces the world image (it is re-created when the viewport resizes).
    void setWorldTexture(ax::Texture2D* texture) { m_worldTexture = texture; }

    /// Makes `texture` drawable by the UI (e.g. a thumbnail) and returns its
    /// id; the same texture always gets the same id. The host keeps a
    /// reference to it until forgetTextures().
    ui::TextureId textureId(ax::Texture2D* texture);
    /// Drops every texture registered with textureId().
    void forgetTextures();

    /// Called with the files dropped onto the window.
    void setDropHandler(std::function<void(const std::vector<std::filesystem::path>&)> handler)
    {
        m_onDrop = std::move(handler);
    }

    /// Updates window size, pixel density and time. Call once per frame
    /// before the application changes the UI.
    void beginFrame(double deltaSeconds);

    /// Lays out and paints the UI, uploads changed textures and updates the
    /// mouse cursor. The result is drawn when the scene is drawn.
    void endFrame();

    /// Called by the render node while the scene is drawn.
    void submit(ax::Renderer* renderer);

    // GLFW event entry points (called from static callbacks).
    void handleKey(int key, int action, int mods);
    void handleChar(unsigned int codepoint);
    void handleMouseButton(int button, int action, int mods);
    void handleCursorPos(double x, double y);
    void handleCursorEnter(bool entered);
    void handleScroll(double x, double y);

private:
    void installCallbacks();
    void restoreCallbacks();
    void uploadAtlas();
    void applyCursor();
    ax::Texture2D* textureFor(ui::TextureId id) const;
    void post(ax::Renderer* renderer, std::function<void()> fn);

    const Logger& m_log;
    GLFWwindow* m_window;
    std::unique_ptr<ui::Context> m_context;
    const ui::DrawList* m_drawList = nullptr;

    UiRenderNode* m_node = nullptr;          // retained; lives in the scene
    ax::Texture2D* m_atlasTexture = nullptr; // owned
    ax::Texture2D* m_worldTexture = nullptr; // owned by the render texture
    std::unordered_map<ui::TextureId, ax::Texture2D*> m_images;      // thumbnails etc. (retained)
    std::unordered_map<ax::Texture2D*, ui::TextureId> m_imageIds;
    ui::TextureId m_nextImageId = 100;
    float m_pixelScale = 1.0f;
    std::uint64_t m_atlasVersion = 0;
    int m_atlasWidth = 0;
    int m_atlasHeight = 0;

    ax::backend::Program* m_program = nullptr;
    ax::backend::VertexLayout m_layout;
    ax::backend::UniformLocation m_mvpLocation;
    ax::backend::UniformLocation m_textureLocation;
    std::vector<std::shared_ptr<ax::CustomCommand>> m_commands;  // kept alive until drawn
    ax::Vector<ax::backend::ProgramState*> m_programStates;

    ui::CursorShape m_cursor = ui::CursorShape::Arrow;
    GLFWcursor* m_cursors[5] = {};
    ui::Modifiers m_mods;
    bool m_hasPointer = false;

    // Axmol's callbacks, called before ours.
    GLFWkeyfun m_prevKey = nullptr;
    GLFWcharfun m_prevChar = nullptr;
    GLFWmousebuttonfun m_prevMouseButton = nullptr;
    GLFWcursorposfun m_prevCursorPos = nullptr;
    GLFWcursorenterfun m_prevCursorEnter = nullptr;
    GLFWscrollfun m_prevScroll = nullptr;
    GLFWdropfun m_prevDrop = nullptr;
    std::function<void(const std::vector<std::filesystem::path>&)> m_onDrop;

    friend struct UiHostCallbacks;
};

} // namespace rolo::detail
