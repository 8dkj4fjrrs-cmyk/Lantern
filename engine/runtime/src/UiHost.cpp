#include "UiHost.hpp"

#include <renderer/CallbackCommand.h>
#include <renderer/CustomCommand.h>
#include <renderer/Renderer.h>
#include <renderer/backend/Buffer.h>
#include <renderer/backend/DriverBase.h>
#include <renderer/backend/ProgramManager.h>
#include <renderer/backend/ProgramState.h>

#include <algorithm>
#include <cmath>

namespace lantern::detail {

namespace {

// Desktop platforms have one main window, so one UI host at a time; GLFW
// callbacks are plain functions and find it through this pointer.
// (Axmol already uses the window's "user pointer" for itself.)
UiHost* g_activeHost = nullptr;

#if defined(__APPLE__)
// macOS trackpads report precise deltas, which GLFW scales by 0.1.
constexpr float kScrollPointsPerUnit = 10.0f;
#else
constexpr float kScrollPointsPerUnit = 40.0f; // one mouse-wheel notch
#endif

ui::Modifiers toModifiers(int mods)
{
    return ui::Modifiers{.shift = (mods & GLFW_MOD_SHIFT) != 0,
                         .ctrl = (mods & GLFW_MOD_CONTROL) != 0,
                         .alt = (mods & GLFW_MOD_ALT) != 0,
                         .super = (mods & GLFW_MOD_SUPER) != 0};
}

ui::Key toKey(int key)
{
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        return static_cast<ui::Key>(static_cast<int>(ui::Key::A) + (key - GLFW_KEY_A));
    }
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        return static_cast<ui::Key>(static_cast<int>(ui::Key::Num0) + (key - GLFW_KEY_0));
    }
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) {
        return static_cast<ui::Key>(static_cast<int>(ui::Key::F1) + (key - GLFW_KEY_F1));
    }
    switch (key) {
    case GLFW_KEY_SPACE: return ui::Key::Space;
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER: return ui::Key::Enter;
    case GLFW_KEY_ESCAPE: return ui::Key::Escape;
    case GLFW_KEY_TAB: return ui::Key::Tab;
    case GLFW_KEY_BACKSPACE: return ui::Key::Backspace;
    case GLFW_KEY_DELETE: return ui::Key::Delete;
    case GLFW_KEY_INSERT: return ui::Key::Insert;
    case GLFW_KEY_LEFT: return ui::Key::Left;
    case GLFW_KEY_RIGHT: return ui::Key::Right;
    case GLFW_KEY_UP: return ui::Key::Up;
    case GLFW_KEY_DOWN: return ui::Key::Down;
    case GLFW_KEY_HOME: return ui::Key::Home;
    case GLFW_KEY_END: return ui::Key::End;
    case GLFW_KEY_PAGE_UP: return ui::Key::PageUp;
    case GLFW_KEY_PAGE_DOWN: return ui::Key::PageDown;
    default: return ui::Key::Unknown;
    }
}

} // namespace

// Plain functions GLFW can call; each forwards to Axmol first, then to us.
struct UiHostCallbacks {
    static void key(GLFWwindow* w, int key, int scancode, int action, int mods)
    {
        if (g_activeHost == nullptr) {
            return;
        }
        if (g_activeHost->m_prevKey != nullptr) {
            g_activeHost->m_prevKey(w, key, scancode, action, mods);
        }
        if (g_activeHost != nullptr) {
            g_activeHost->handleKey(key, action, mods);
        }
    }
    static void character(GLFWwindow* w, unsigned int codepoint)
    {
        if (g_activeHost == nullptr) {
            return;
        }
        if (g_activeHost->m_prevChar != nullptr) {
            g_activeHost->m_prevChar(w, codepoint);
        }
        if (g_activeHost != nullptr) {
            g_activeHost->handleChar(codepoint);
        }
    }
    static void mouseButton(GLFWwindow* w, int button, int action, int mods)
    {
        if (g_activeHost == nullptr) {
            return;
        }
        if (g_activeHost->m_prevMouseButton != nullptr) {
            g_activeHost->m_prevMouseButton(w, button, action, mods);
        }
        if (g_activeHost != nullptr) {
            g_activeHost->handleMouseButton(button, action, mods);
        }
    }
    static void cursorPos(GLFWwindow* w, double x, double y)
    {
        if (g_activeHost == nullptr) {
            return;
        }
        if (g_activeHost->m_prevCursorPos != nullptr) {
            g_activeHost->m_prevCursorPos(w, x, y);
        }
        if (g_activeHost != nullptr) {
            g_activeHost->handleCursorPos(x, y);
        }
    }
    static void cursorEnter(GLFWwindow* w, int entered)
    {
        if (g_activeHost == nullptr) {
            return;
        }
        if (g_activeHost->m_prevCursorEnter != nullptr) {
            g_activeHost->m_prevCursorEnter(w, entered);
        }
        if (g_activeHost != nullptr) {
            g_activeHost->handleCursorEnter(entered != 0);
        }
    }
    static void scroll(GLFWwindow* w, double x, double y)
    {
        if (g_activeHost == nullptr) {
            return;
        }
        if (g_activeHost->m_prevScroll != nullptr) {
            g_activeHost->m_prevScroll(w, x, y);
        }
        if (g_activeHost != nullptr) {
            g_activeHost->handleScroll(x, y);
        }
    }
};

// Draws the UI on top of everything else in the scene.
class UiRenderNode final : public ax::Node {
public:
    explicit UiRenderNode(UiHost* host)
        : m_host(host)
    {
    }
    void detach() { m_host = nullptr; }
    void draw(ax::Renderer* renderer, const ax::Mat4&, uint32_t) override
    {
        if (m_host != nullptr) {
            m_host->submit(renderer);
        }
    }

private:
    UiHost* m_host;
};

// --- Setup and teardown ------------------------------------------------------------------

UiHost::UiHost(const Logger& logger, GLFWwindow* window, ax::Scene& scene, ax::Texture2D* worldTexture)
    : m_log(logger)
    , m_window(window)
    , m_context(std::make_unique<ui::Context>())
    , m_worldTexture(worldTexture)
{
    // Shader: Axmol's built-in "position + texture + colour" program.
    m_program = ax::backend::ProgramManager::getInstance()->getBuiltinProgram(
        ax::backend::ProgramType::POSITION_TEXTURE_COLOR);
    m_mvpLocation = m_program->getUniformLocation(ax::backend::Uniform::MVP_MATRIX);
    m_textureLocation = m_program->getUniformLocation(ax::backend::Uniform::TEXTURE);
    m_layout.setAttrib("a_position", static_cast<std::size_t>(m_program->getAttributeLocation(ax::backend::Attribute::POSITION)),
                       ax::backend::VertexFormat::FLOAT2, offsetof(ui::Vertex, x), false);
    m_layout.setAttrib("a_texCoord", static_cast<std::size_t>(m_program->getAttributeLocation(ax::backend::Attribute::TEXCOORD)),
                       ax::backend::VertexFormat::FLOAT2, offsetof(ui::Vertex, u), false);
    m_layout.setAttrib("a_color", static_cast<std::size_t>(m_program->getAttributeLocation(ax::backend::Attribute::COLOR)),
                       ax::backend::VertexFormat::UBYTE4, offsetof(ui::Vertex, color), true);
    m_layout.setStride(sizeof(ui::Vertex));

    // The node that draws the UI, above the world and anything else.
    m_node = new UiRenderNode(this);
    m_node->init();
    m_node->setName("LanternUI");
    scene.addChild(m_node, 1000); // the scene retains it...
    // (m_node keeps its initial reference too, released in ~UiHost)

    // Clipboard and platform conventions.
    m_context->platform().getClipboard = [this] {
        const char* text = glfwGetClipboardString(m_window);
        return std::string(text != nullptr ? text : "");
    };
    m_context->platform().setClipboard = [this](std::string_view text) {
        glfwSetClipboardString(m_window, std::string(text).c_str());
    };

    m_cursors[static_cast<int>(ui::CursorShape::Arrow)] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    m_cursors[static_cast<int>(ui::CursorShape::IBeam)] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    m_cursors[static_cast<int>(ui::CursorShape::Hand)] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    m_cursors[static_cast<int>(ui::CursorShape::ResizeHorizontal)] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    m_cursors[static_cast<int>(ui::CursorShape::ResizeVertical)] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);

    installCallbacks();
}

UiHost::~UiHost()
{
    restoreCallbacks();
    for (GLFWcursor*& cursor : m_cursors) {
        if (cursor != nullptr) {
            glfwDestroyCursor(cursor);
            cursor = nullptr;
        }
    }
    if (m_node != nullptr) {
        m_node->detach();
        m_node->release();
    }
    m_commands.clear();
    m_programStates.clear();
    AX_SAFE_RELEASE_NULL(m_atlasTexture);
}

void UiHost::installCallbacks()
{
    g_activeHost = this;
    m_prevKey = glfwSetKeyCallback(m_window, &UiHostCallbacks::key);
    m_prevChar = glfwSetCharCallback(m_window, &UiHostCallbacks::character);
    m_prevMouseButton = glfwSetMouseButtonCallback(m_window, &UiHostCallbacks::mouseButton);
    m_prevCursorPos = glfwSetCursorPosCallback(m_window, &UiHostCallbacks::cursorPos);
    m_prevCursorEnter = glfwSetCursorEnterCallback(m_window, &UiHostCallbacks::cursorEnter);
    m_prevScroll = glfwSetScrollCallback(m_window, &UiHostCallbacks::scroll);
}

void UiHost::restoreCallbacks()
{
    if (g_activeHost != this) {
        return;
    }
    glfwSetKeyCallback(m_window, m_prevKey);
    glfwSetCharCallback(m_window, m_prevChar);
    glfwSetMouseButtonCallback(m_window, m_prevMouseButton);
    glfwSetCursorPosCallback(m_window, m_prevCursorPos);
    glfwSetCursorEnterCallback(m_window, m_prevCursorEnter);
    glfwSetScrollCallback(m_window, m_prevScroll);
    g_activeHost = nullptr;
}

// --- Input -------------------------------------------------------------------------------

void UiHost::handleKey(int key, int action, int mods)
{
    m_mods = toModifiers(mods);
    const ui::Key k = toKey(key);
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (k != ui::Key::Unknown) {
            m_context->keyDown(k, m_mods, action == GLFW_REPEAT);
        }
    } else if (action == GLFW_RELEASE) {
        m_context->keyUp(k, m_mods);
    }
}

void UiHost::handleChar(unsigned int codepoint)
{
    m_context->textInput(static_cast<char32_t>(codepoint));
}

void UiHost::handleMouseButton(int button, int action, int mods)
{
    ui::MouseButton b = ui::MouseButton::Left;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        b = ui::MouseButton::Right;
    } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        b = ui::MouseButton::Middle;
    } else if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    m_mods = toModifiers(mods);
    m_context->pointerButton(b, action == GLFW_PRESS, m_mods);
    applyCursor();
}

void UiHost::handleCursorPos(double x, double y)
{
    m_hasPointer = true;
    m_context->pointerMove({static_cast<float>(x), static_cast<float>(y)});
    applyCursor();
}

void UiHost::handleCursorEnter(bool entered)
{
    if (!entered) {
        m_hasPointer = false;
        m_context->pointerLeftWindow();
        applyCursor();
    }
}

void UiHost::handleScroll(double x, double y)
{
    m_context->scroll({static_cast<float>(x) * kScrollPointsPerUnit, static_cast<float>(y) * kScrollPointsPerUnit});
}

void UiHost::applyCursor()
{
    const ui::CursorShape shape = m_context->cursor();
    if (shape != m_cursor) {
        m_cursor = shape;
        glfwSetCursor(m_window, m_cursors[static_cast<int>(shape)]);
    }
}

// --- Frame -------------------------------------------------------------------------------

void UiHost::beginFrame(double deltaSeconds)
{
    int windowW = 0;
    int windowH = 0;
    int framebufferW = 0;
    int framebufferH = 0;
    glfwGetWindowSize(m_window, &windowW, &windowH);
    glfwGetFramebufferSize(m_window, &framebufferW, &framebufferH);
    const float scale = windowW > 0 ? static_cast<float>(framebufferW) / static_cast<float>(windowW) : 1.0f;
    m_context->setViewport({static_cast<float>(windowW), static_cast<float>(windowH)}, scale);
    m_context->advanceTime(deltaSeconds);
}

void UiHost::endFrame()
{
    m_drawList = &m_context->render();
    uploadAtlas();
    applyCursor();
}

void UiHost::uploadAtlas()
{
    const ui::FontAtlas& atlas = m_context->atlas();
    if (m_atlasTexture != nullptr && atlas.version() == m_atlasVersion) {
        return;
    }
    const auto bytes = static_cast<ssize_t>(atlas.pixels().size());
    if (m_atlasTexture == nullptr || atlas.width() != m_atlasWidth || atlas.height() != m_atlasHeight) {
        AX_SAFE_RELEASE_NULL(m_atlasTexture);
        m_atlasTexture = new ax::Texture2D();
        m_atlasTexture->initWithData(atlas.pixels().data(), bytes, ax::backend::PixelFormat::RGBA8, atlas.width(),
                                     atlas.height(), false);
        ax::backend::SamplerDescriptor sampler(ax::backend::SamplerFilter::LINEAR, ax::backend::SamplerFilter::LINEAR,
                                               ax::backend::SamplerAddressMode::CLAMP_TO_EDGE,
                                               ax::backend::SamplerAddressMode::CLAMP_TO_EDGE);
        m_atlasTexture->getBackendTexture()->updateSamplerDescriptor(sampler);
        m_atlasWidth = atlas.width();
        m_atlasHeight = atlas.height();
        m_log.debug("ui", "Glyph atlas texture: " + std::to_string(m_atlasWidth) + "x" + std::to_string(m_atlasHeight)
                              + " pixels");
    } else {
        m_atlasTexture->updateWithSubData(const_cast<std::uint8_t*>(atlas.pixels().data()), 0, 0, atlas.width(),
                                          atlas.height());
    }
    m_atlasVersion = atlas.version();
}

ax::Texture2D* UiHost::textureFor(ui::TextureId id) const
{
    if (id == ui::kAtlasTexture) {
        return m_atlasTexture;
    }
    if (id == kWorldTexture) {
        return m_worldTexture;
    }
    return nullptr;
}

void UiHost::post(ax::Renderer* renderer, std::function<void()> fn)
{
    ax::CallbackCommand* cmd = renderer->nextCallbackCommand();
    cmd->init(0.0f);
    cmd->func = std::move(fn);
    renderer->addCommand(cmd);
}

// --- Drawing ----------------------------------------------------------------------------

void UiHost::submit(ax::Renderer* renderer)
{
    // Last frame's commands have been drawn by now; free them.
    m_commands.clear();
    m_programStates.clear();

    if (m_drawList == nullptr || m_drawList->indices().empty() || m_atlasTexture == nullptr) {
        return;
    }
    int fbW = 0;
    int fbH = 0;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    const ui::Size viewport = m_context->viewport();
    if (fbW <= 0 || fbH <= 0 || viewport.width <= 0.0f) {
        return;
    }
    const float scale = static_cast<float>(fbW) / viewport.width;

    // Upload this frame's triangles.
    auto* driver = ax::backend::DriverBase::getInstance();
    const auto& vertices = m_drawList->vertices();
    const auto& indices = m_drawList->indices();
    auto* vbuffer = driver->newBuffer(vertices.size() * sizeof(ui::Vertex), ax::backend::BufferType::VERTEX,
                                      ax::backend::BufferUsage::STATIC);
    vbuffer->autorelease();
    vbuffer->updateData(vertices.data(), vertices.size() * sizeof(ui::Vertex));
    auto* ibuffer = driver->newBuffer(indices.size() * sizeof(std::uint32_t), ax::backend::BufferType::INDEX,
                                      ax::backend::BufferUsage::STATIC);
    ibuffer->autorelease();
    ibuffer->updateData(indices.data(), indices.size() * sizeof(std::uint32_t));

    // Our vertices are in points with y down: map the window onto clip space.
    ax::Mat4 projection;
    ax::Mat4::createOrthographicOffCenter(0.0f, viewport.width, viewport.height, 0.0f, -1.0f, 1.0f, &projection);

    // Save and set render state: whole framebuffer, scissor on, no culling/depth.
    struct Saved {
        ax::backend::CullMode cull;
        ax::Viewport viewport;
        bool scissorTest;
        ax::ScissorRect scissor;
        bool depthTest;
    };
    auto saved = std::make_shared<Saved>();
    post(renderer, [renderer, saved, fbW, fbH] {
        saved->cull = renderer->getCullMode();
        saved->viewport = renderer->getViewport();
        saved->scissorTest = renderer->getScissorTest();
        saved->scissor = renderer->getScissorRect();
        saved->depthTest = renderer->getDepthTest();
        renderer->setCullMode(ax::backend::CullMode::NONE);
        renderer->setDepthTest(false);
        renderer->setScissorTest(true);
        renderer->setViewPort(0, 0, static_cast<unsigned int>(fbW), static_cast<unsigned int>(fbH));
    });

    for (const ui::DrawCommand& dc : m_drawList->commands()) {
        ax::Texture2D* texture = textureFor(dc.texture);
        if (texture == nullptr || dc.indexCount == 0) {
            continue;
        }
        // Clip rectangle in framebuffer pixels (Axmol's scissor origin is bottom-left).
        const float x0 = std::floor(dc.clip.x * scale);
        const float y0 = std::floor(dc.clip.y * scale);
        const float x1 = std::ceil(dc.clip.right() * scale);
        const float y1 = std::ceil(dc.clip.bottom() * scale);
        if (x1 <= x0 || y1 <= y0) {
            continue;
        }
        post(renderer, [renderer, x0, y1, x1, y0, fbH] {
            renderer->setScissorRect(x0, static_cast<float>(fbH) - y1, x1 - x0, y1 - y0);
        });

        auto command = std::make_shared<ax::CustomCommand>();
        command->init(0.0f, ax::BlendFunc::ALPHA_NON_PREMULTIPLIED);
        auto* state = new ax::backend::ProgramState(m_program);
        state->autorelease();
        m_programStates.pushBack(state);
        auto& pipeline = command->getPipelineDescriptor();
        pipeline.programState = state;
        state->setSharedVertexLayout(&m_layout);
        state->setUniform(m_mvpLocation, &projection, sizeof(projection));
        state->setTexture(m_textureLocation, 0, texture->getBackendTexture());
        pipeline.blendDescriptor.sourceAlphaBlendFactor = ax::backend::BlendFactor::ONE; // keep alpha opaque
        command->setIndexBuffer(ibuffer, ax::backend::IndexFormat::U_INT);
        command->setVertexBuffer(vbuffer);
        command->setDrawType(ax::CustomCommand::DrawType::ELEMENT);
        command->setPrimitiveType(ax::backend::PrimitiveType::TRIANGLE);
        command->setIndexDrawInfo(dc.indexOffset, dc.indexCount);
        renderer->addCommand(command.get());
        m_commands.push_back(std::move(command));
    }

    post(renderer, [renderer, saved] {
        renderer->setCullMode(saved->cull);
        renderer->setViewPort(saved->viewport.x, saved->viewport.y, static_cast<unsigned int>(saved->viewport.w),
                              static_cast<unsigned int>(saved->viewport.h));
        renderer->setScissorTest(saved->scissorTest);
        renderer->setScissorRect(static_cast<float>(saved->scissor.x), static_cast<float>(saved->scissor.y),
                                 static_cast<float>(saved->scissor.width), static_cast<float>(saved->scissor.height));
        renderer->setDepthTest(saved->depthTest);
        // Applied by the renderer when it starts the next draw.
    });
}

} // namespace lantern::detail
