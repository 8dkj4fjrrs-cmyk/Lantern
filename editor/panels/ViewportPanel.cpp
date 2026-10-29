#include "Panels.hpp"

#include "lantern/ui/Context.hpp"

#include <array>
#include <cmath>
#include <string>

namespace lantern::editor {

// The world image, plus selection: click an object to select it; the
// selected object gets an outline.
class ViewportPanel::SceneView final : public ui::ImageView {
public:
    explicit SceneView(EditorServices& editor)
        : ui::ImageView(0, {})
        , m_editor(editor)
    {
        setBackground(ui::Color::hex(0x111111));
        setStretch(1.0f);
    }

    void setWorldSize(float width, float height)
    {
        m_worldWidth = width;
        m_worldHeight = height;
    }

    // World coordinates (y up, origin bottom-left) <-> panel points (y down).
    ui::Vec2 toScreen(Vec2 world) const
    {
        const ui::Rect image = imageRect();
        const float s = imageScale();
        return {image.x + world.x * s, image.y + (m_worldHeight - world.y) * s};
    }
    Vec2 toWorld(ui::Vec2 screen) const
    {
        const ui::Rect image = imageRect();
        const float s = imageScale() > 0.0f ? imageScale() : 1.0f;
        return {(screen.x - image.x) / s, m_worldHeight - (screen.y - image.y) / s};
    }

    bool onPointerDown(ui::Context&, const ui::PointerEvent& event) override
    {
        if (event.button != ui::MouseButton::Left) {
            return false;
        }
        if (!imageRect().contains(event.position)) {
            m_editor.select({});
            return true;
        }
        m_editor.select(m_editor.document().scene().hitTest(toWorld(event.position)));
        return true;
    }

    void paint(ui::Context& ctx, ui::DrawList& draw) override
    {
        ImageView::paint(ctx, draw);
        const EntityId selected = m_editor.selection();
        const Scene& scene = m_editor.document().scene();
        if (selected.isNull() || !scene.contains(selected)) {
            return;
        }
        const ui::Theme& t = ctx.theme();
        draw.pushClip(imageRect());
        if (const auto corners = scene.worldCorners(selected)) {
            std::array<ui::Vec2, 4> points{};
            for (std::size_t i = 0; i < 4; ++i) {
                points[i] = toScreen((*corners)[i]);
            }
            draw.polyline(points, ui::Color{0, 0, 0, 120}, 3.5f, true);
            draw.polyline(points, t.accent, 1.5f, true);
            for (const ui::Vec2& p : points) {
                draw.fillRect(ui::Rect{p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f}, ui::Color{0, 0, 0, 140});
                draw.fillRect(ui::Rect{p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f}, t.accent);
            }
        }
        // The object's origin (its position), also for objects without a shape.
        const Vec2 originWorld = scene.worldTransform(selected).apply({0.0f, 0.0f});
        const ui::Vec2 o = toScreen(originWorld);
        draw.line({o.x - 7.0f, o.y}, {o.x + 7.0f, o.y}, t.accent, 1.5f);
        draw.line({o.x, o.y - 7.0f}, {o.x, o.y + 7.0f}, t.accent, 1.5f);
        draw.popClip();
    }

private:
    EditorServices& m_editor;
    float m_worldWidth = 1280.0f;
    float m_worldHeight = 720.0f;
};

ViewportPanel::ViewportPanel(EditorServices& editor)
    : ui::Column(0.0f)
{
    // Tool strip: the scene editing tools, then the game's resolution and zoom.
    auto& toolbar = makePanelToolbar(*this);
    addToolButton(toolbar, ui::Icon::Move, std::string("Move tool. ") + kComingWithViewportTools);
    addToolButton(toolbar, ui::Icon::Rotate, std::string("Rotate tool. ") + kComingWithViewportTools);
    addToolButton(toolbar, ui::Icon::Scale, std::string("Scale tool. ") + kComingWithViewportTools);
    addToolButton(toolbar, ui::Icon::RectTool, std::string("Rect tool. ") + kComingWithViewportTools);
    toolbar.emplace<ui::Spacer>();
    m_zoom = &toolbar.emplace<ui::Label>("", ui::Label::Style::Dim);
    m_zoom->setTooltip("The game's resolution, and how much it is scaled to fit this panel. "
                       "Zooming and panning arrive in Milestone 4.");
    toolbar.emplace<ui::Spacer>(0.0f).setFixedSize({6.0f, 1.0f});

    m_view = &addChild(std::make_unique<SceneView>(editor));
}

void ViewportPanel::update(UiFrame& frame)
{
    const WorldImage image = frame.worldImage();
    m_view->setTexture(image.texture, image.size);
    m_view->setUv(image.uv0, image.uv1);
    m_view->setWorldSize(static_cast<float>(frame.worldWidth()), static_cast<float>(frame.worldHeight()));

    const int percent = static_cast<int>(std::lround(m_view->imageScale() * 100.0f));
    m_zoom->setText(std::to_string(frame.worldWidth()) + " × " + std::to_string(frame.worldHeight())
                    + "   ·   " + std::to_string(percent) + "%");
}

} // namespace lantern::editor
