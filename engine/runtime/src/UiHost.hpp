#pragma once

// UiHost: connects a lantern::ui::Context to the real window.
//
//   input    GLFW callbacks (keys, typed text, mouse, scroll) → Context events.
//            Axmol's own GLFW callbacks are kept and still called first.
//   output   the Context's DrawList → Axmol render commands (triangles,
//            textures, clipping), drawn on top of the scene.
//   extras   clipboard, mouse cursor shapes, the glyph-atlas texture.
//
// Internal to lantern_runtime.

#include "lantern/core/Log.hpp"
#include "lantern/ui/Context.hpp"

#include <axmol.h>                 // must come first: it sets up the OpenGL loader
#include <platform/RenderViewImpl.h> // brings in GLFW the way Axmol expects
#include <renderer/backend/VertexLayout.h>

#include <memory>
#include <vector>

namespace lantern::detail {

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

    friend struct UiHostCallbacks;
};

} // namespace lantern::detail
