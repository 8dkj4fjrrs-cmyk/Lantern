#include "Panels.hpp"

#include "../model/Commands.hpp"

#include "lantern/ui/Context.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace lantern::editor {

namespace {

constexpr float kMinZoom = 0.05f;    // screen points per world point
constexpr float kMaxZoom = 32.0f;
constexpr float kGizmoLength = 64.0f; // screen points
constexpr float kHandleSize = 9.0f;
constexpr float kPi = 3.14159265358979f;

float length(ui::Vec2 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

ui::Vec2 normalized(ui::Vec2 v)
{
    const float l = length(v);
    return l > 1e-5f ? ui::Vec2{v.x / l, v.y / l} : ui::Vec2{1.0f, 0.0f};
}

// Distance from p to the segment a-b.
float distanceToSegment(ui::Vec2 p, ui::Vec2 a, ui::Vec2 b)
{
    const ui::Vec2 ab{b.x - a.x, b.y - a.y};
    const float len2 = ab.x * ab.x + ab.y * ab.y;
    const float t = len2 > 0.0f ? std::clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2, 0.0f, 1.0f) : 0.0f;
    return length({p.x - (a.x + ab.x * t), p.y - (a.y + ab.y * t)});
}

// The grid spacing (world points, a power of two) that keeps lines at least
// ~20 screen points apart at this zoom.
float gridStep(float zoom)
{
    float step = 1.0f;
    while (step * zoom < 20.0f && step < 65536.0f) {
        step *= 2.0f;
    }
    return step;
}

float roundTo(float value, float step)
{
    return step > 0.0f ? std::round(value / step) * step : value;
}

std::string percentText(float zoom)
{
    return std::to_string(static_cast<int>(std::lround(zoom * 100.0f))) + "%";
}

// The size property of the component that gives an entity its bounds.
const char* sizeComponentOf(const Entity& entity)
{
    if (entity.get<Shape>() != nullptr) {
        return "lantern.Shape";
    }
    if (entity.get<Sprite>() != nullptr) {
        return "lantern.Sprite";
    }
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// SceneView: the world image plus the editor camera, grid, selection and tools.
// ---------------------------------------------------------------------------
class ViewportPanel::SceneView final : public ui::Widget {
public:
    SceneView(EditorServices& editor, ViewportPanel& panel)
        : m_editor(editor)
        , m_panel(panel)
    {
        setStretch(1.0f);
        setFocusable(true);
    }

    // --- Camera ------------------------------------------------------------------------
    Vec2 center() const noexcept { return m_center; }
    float zoom() const noexcept { return m_zoom; }

    // World coordinates (y up) <-> panel points (y down).
    ui::Vec2 toScreen(Vec2 world) const
    {
        const ui::Vec2 c = bounds().center();
        return {c.x + (world.x - m_center.x) * m_zoom, c.y - (world.y - m_center.y) * m_zoom};
    }
    Vec2 toWorld(ui::Vec2 screen) const
    {
        const ui::Vec2 c = bounds().center();
        return {m_center.x + (screen.x - c.x) / m_zoom, m_center.y - (screen.y - c.y) / m_zoom};
    }

    void setZoomAround(float zoom, ui::Vec2 screenPoint)
    {
        const Vec2 anchor = toWorld(screenPoint);
        m_zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
        const ui::Vec2 c = bounds().center();
        m_center = {anchor.x - (screenPoint.x - c.x) / m_zoom, anchor.y + (screenPoint.y - c.y) / m_zoom};
    }

    /// Shows the world rectangle (min corner, size) with a margin.
    void showArea(Vec2 min, Vec2 size, float margin = 0.9f, float maxZoom = kMaxZoom)
    {
        const ui::Rect b = bounds();
        if (b.width <= 0.0f || b.height <= 0.0f) {
            m_pendingFit = true;
            return;
        }
        const float sx = size.x > 0.0f ? b.width / size.x : kMaxZoom;
        const float sy = size.y > 0.0f ? b.height / size.y : kMaxZoom;
        m_zoom = std::clamp(std::min(sx, sy) * margin, kMinZoom, maxZoom);
        m_center = {min.x + size.x * 0.5f, min.y + size.y * 0.5f};
    }

    /// The area the player sees: the scene's camera, or (0, 0) to the game's resolution.
    std::pair<Vec2, Vec2> gameArea() const
    {
        if (m_gameCamera) {
            const Vec2 size{m_worldWidth / m_gameCamera->zoom, m_worldHeight / m_gameCamera->zoom};
            return {{m_gameCamera->center.x - size.x * 0.5f, m_gameCamera->center.y - size.y * 0.5f}, size};
        }
        return {{0.0f, 0.0f}, {m_worldWidth, m_worldHeight}};
    }

    void fitGameView()
    {
        const auto [min, size] = gameArea();
        showArea(min, size);
    }

    void frameSelection()
    {
        const Scene& scene = m_editor.document().scene();
        const EntityId id = m_editor.selection();
        if (id.isNull() || !scene.contains(id)) {
            fitGameView();
            return;
        }
        if (const auto corners = scene.worldCorners(id)) {
            Vec2 lo = (*corners)[0];
            Vec2 hi = lo;
            for (const Vec2& p : *corners) {
                lo = {std::min(lo.x, p.x), std::min(lo.y, p.y)};
                hi = {std::max(hi.x, p.x), std::max(hi.y, p.y)};
            }
            showArea(lo, hi - lo, 0.5f, 8.0f);
        } else {
            m_center = scene.worldTransform(id).apply({0.0f, 0.0f});
        }
    }

    void setFrameData(const WorldImage& image, float worldWidth, float worldHeight,
                      std::optional<GameCameraView> camera)
    {
        m_image = image;
        m_worldWidth = worldWidth;
        m_worldHeight = worldHeight;
        m_gameCamera = camera;
    }

    // --- Layout and painting ------------------------------------------------------------
    void arrange(ui::Context& ctx, ui::Rect rect) override
    {
        Widget::arrange(ctx, rect);
        if (m_pendingFit && !rect.isEmpty()) {
            m_pendingFit = false;
            fitGameView();
        }
    }

    void paint(ui::Context& ctx, ui::DrawList& draw) override
    {
        const ui::Theme& t = ctx.theme();
        const ui::Rect b = bounds();
        draw.fillRect(b, ui::Color::hex(0x1B1B1D));
        draw.pushClip(b);

        // The world image is rendered at this panel's size; until a resize has
        // been rendered, show it at its own size, centred.
        if (m_image.texture != 0) {
            const ui::Vec2 c = b.center();
            const ui::Rect r{std::round(c.x - m_image.size.width * 0.5f), std::round(c.y - m_image.size.height * 0.5f),
                             m_image.size.width, m_image.size.height};
            draw.image(m_image.texture, r, m_image.uv0, m_image.uv1);
        }
        paintGrid(draw);
        paintGameFrame(ctx, draw);
        paintSelection(ctx, draw);
        draw.popClip();
        if (isFocused()) {
            draw.strokeRoundedRect(b.inset(1.0f), 0.0f, t.focusRing.fade(0.5f));
        }
    }

    // --- Input ------------------------------------------------------------------------------
    bool onScroll(ui::Context& ctx, const ui::ScrollEvent& event) override
    {
        // Up/away zooms in, around the mouse; a wheel notch is about 20%.
        const float factor = std::clamp(std::pow(1.005f, event.delta.y), 0.5f, 2.0f);
        setZoomAround(m_zoom * factor, ctx.pointerPosition());
        return true;
    }

    bool onPointerDown(ui::Context& ctx, const ui::PointerEvent& event) override
    {
        ctx.setFocus(this);
        m_pressScreen = event.position;
        const bool panButton = event.button == ui::MouseButton::Right || event.button == ui::MouseButton::Middle
                               || (event.button == ui::MouseButton::Left && event.modifiers.alt);
        if (panButton) {
            m_drag = Drag::Pan;
            m_panStartCenter = m_center;
            return true;
        }
        if (event.button != ui::MouseButton::Left) {
            return false;
        }
        const Scene& scene = m_editor.document().scene();
        // A handle of the selected object's tool?
        const Handle handle = handleAt(event.position);
        if (handle != Handle::None) {
            beginTransformDrag(handle, event.position);
            return true;
        }
        // Otherwise select what was clicked (or nothing), and drag it to move it.
        const EntityId hit = scene.hitTest(toWorld(event.position));
        m_editor.select(hit);
        if (!hit.isNull()) {
            beginTransformDrag(Handle::Body, event.position);
        }
        return true;
    }

    void onPointerMove(ui::Context&, const ui::PointerEvent& event) override
    {
        if (m_drag == Drag::Pan) {
            m_center = {m_panStartCenter.x - (event.position.x - m_pressScreen.x) / m_zoom,
                        m_panStartCenter.y + (event.position.y - m_pressScreen.y) / m_zoom};
        } else if (m_drag == Drag::Transform) {
            // A click on an object should select it, not nudge it: wait for a real drag.
            if (!m_moved && length({event.position.x - m_pressScreen.x, event.position.y - m_pressScreen.y}) < 3.0f) {
                return;
            }
            m_moved = true;
            dragTo(event.position, event.modifiers);
        }
    }

    bool onPointerUp(ui::Context&, const ui::PointerEvent&) override
    {
        if (m_drag == Drag::Transform && m_moved) {
            m_editor.endEdit();
        }
        m_drag = Drag::None;
        m_moved = false;
        return true;
    }

    bool onKeyDown(ui::Context& ctx, const ui::KeyEvent& event) override
    {
        const ui::Modifiers& m = event.modifiers;
        if (m.ctrl || m.super || m.alt) {
            return false;
        }
        switch (event.key) {
        case ui::Key::W: m_panel.setTool(SceneTool::Move); return true;
        case ui::Key::E: m_panel.setTool(SceneTool::Rotate); return true;
        case ui::Key::R: m_panel.setTool(SceneTool::Scale); return true;
        case ui::Key::T: m_panel.setTool(SceneTool::Rect); return true;
        case ui::Key::F: frameSelection(); return true;
        case ui::Key::Delete:
        case ui::Key::Backspace: m_editor.deleteSelection(); return true;
        case ui::Key::Left: nudge({m.shift ? -10.0f : -1.0f, 0.0f}); return true;
        case ui::Key::Right: nudge({m.shift ? 10.0f : 1.0f, 0.0f}); return true;
        case ui::Key::Up: nudge({0.0f, m.shift ? 10.0f : 1.0f}); return true;
        case ui::Key::Down: nudge({0.0f, m.shift ? -10.0f : -1.0f}); return true;
        default: break;
        }
        static_cast<void>(ctx);
        return false;
    }

    ui::CursorShape cursor(ui::Vec2 point) const override
    {
        if (m_drag == Drag::Pan) {
            return ui::CursorShape::Hand;
        }
        return handleAt(point) != Handle::None ? ui::CursorShape::Hand : ui::CursorShape::Arrow;
    }

    std::string tooltip() const override
    {
        return {}; // the tools explain themselves in the toolbar
    }

private:
    enum class Drag { None, Pan, Transform };
    enum class Handle {
        None,
        Body,            // the object itself: move freely
        MoveX, MoveY, MoveXY,
        Rotate,
        ScaleX, ScaleY, ScaleXY,
        // Rect tool: corners and edges, in worldCorners() order where possible.
        RectBL, RectBR, RectTR, RectTL, RectB, RectR, RectT, RectL,
    };

    struct DragStart {
        EntityId id;
        Handle handle = Handle::None;
        Vec2 pointerWorld;
        Vec2 position;
        float rotation = 0.0f;
        Vec2 scale;
        Vec2 size;
        const char* sizeComponent = nullptr;
        Affine2 world;           // the entity's world transform
        Affine2 parentInverse;   // world -> parent space
        Vec2 worldOrigin;
        ui::Vec2 screenOrigin;
        float startAngle = 0.0f; // Rotate: screen angle of the pointer
    };

    // --- Gizmo geometry (screen space) --------------------------------------------------------
    struct Gizmo {
        ui::Vec2 origin;
        ui::Vec2 axisX;   // unit directions on screen
        ui::Vec2 axisY;
    };

    std::optional<Gizmo> gizmo() const
    {
        const Scene& scene = m_editor.document().scene();
        const EntityId id = m_editor.selection();
        if (id.isNull() || !scene.contains(id)) {
            return std::nullopt;
        }
        const Affine2 world = scene.worldTransform(id);
        Gizmo g;
        g.origin = toScreen(world.apply({0.0f, 0.0f}));
        if (m_panel.tool() == SceneTool::Scale) {
            // Scale works along the object's own axes: its local x axis is
            // (a, b) in the world, its y axis (c, d); screen y points down.
            g.axisX = normalized({world.a, -world.b});
            g.axisY = normalized({world.c, -world.d});
        } else {
            g.axisX = {1.0f, 0.0f};
            g.axisY = {0.0f, -1.0f}; // world up = screen up
        }
        return g;
    }

    std::vector<std::pair<Handle, ui::Vec2>> rectHandles() const
    {
        const Scene& scene = m_editor.document().scene();
        const auto corners = scene.worldCorners(m_editor.selection());
        if (!corners) {
            return {};
        }
        std::array<ui::Vec2, 4> s{};
        for (std::size_t i = 0; i < 4; ++i) {
            s[i] = toScreen((*corners)[i]);
        }
        const auto mid = [](ui::Vec2 a, ui::Vec2 b) { return ui::Vec2{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f}; };
        return {{Handle::RectBL, s[0]},           {Handle::RectBR, s[1]},           {Handle::RectTR, s[2]},
                {Handle::RectTL, s[3]},           {Handle::RectB, mid(s[0], s[1])}, {Handle::RectR, mid(s[1], s[2])},
                {Handle::RectT, mid(s[2], s[3])}, {Handle::RectL, mid(s[3], s[0])}};
    }

    Handle handleAt(ui::Vec2 p) const
    {
        const auto g = gizmo();
        if (!g) {
            return Handle::None;
        }
        const ui::Vec2 o = g->origin;
        const auto end = [&](ui::Vec2 axis) { return ui::Vec2{o.x + axis.x * kGizmoLength, o.y + axis.y * kGizmoLength}; };
        const auto near = [&](ui::Vec2 q, float r) { return length({p.x - q.x, p.y - q.y}) <= r; };
        switch (m_panel.tool()) {
        case SceneTool::Move:
            if (near({o.x + 12.0f, o.y - 12.0f}, 9.0f)) {
                return Handle::MoveXY;
            }
            if (distanceToSegment(p, {o.x + 14.0f, o.y}, end(g->axisX)) <= 6.0f) {
                return Handle::MoveX;
            }
            if (distanceToSegment(p, {o.x, o.y - 14.0f}, end(g->axisY)) <= 6.0f) {
                return Handle::MoveY;
            }
            break;
        case SceneTool::Rotate: {
            const float d = length({p.x - o.x, p.y - o.y});
            if (std::abs(d - kGizmoLength) <= 7.0f) {
                return Handle::Rotate;
            }
            break;
        }
        case SceneTool::Scale:
            if (near(o, 9.0f)) {
                return Handle::ScaleXY;
            }
            if (near(end(g->axisX), 9.0f) || distanceToSegment(p, o, end(g->axisX)) <= 5.0f) {
                return Handle::ScaleX;
            }
            if (near(end(g->axisY), 9.0f) || distanceToSegment(p, o, end(g->axisY)) <= 5.0f) {
                return Handle::ScaleY;
            }
            break;
        case SceneTool::Rect:
            for (const auto& [h, pos] : rectHandles()) {
                if (near(pos, kHandleSize)) {
                    return h;
                }
            }
            break;
        }
        return Handle::None;
    }

    // --- Dragging -----------------------------------------------------------------------------
    void beginTransformDrag(Handle handle, ui::Vec2 screen)
    {
        const Scene& scene = m_editor.document().scene();
        const EntityId id = m_editor.selection();
        const Entity* e = scene.find(id);
        if (e == nullptr) {
            return;
        }
        m_editor.endEdit(); // a new drag is a new undo step
        DragStart d;
        d.id = id;
        d.handle = handle;
        d.pointerWorld = toWorld(screen);
        const Transform& t = e->transform();
        d.position = t.position;
        d.rotation = t.rotation;
        d.scale = t.scale;
        d.world = scene.worldTransform(id);
        const Affine2 parentWorld = e->parent().isNull() ? Affine2{} : scene.worldTransform(e->parent());
        d.parentInverse = parentWorld.inverse().value_or(Affine2{});
        d.worldOrigin = d.world.apply({0.0f, 0.0f});
        d.screenOrigin = toScreen(d.worldOrigin);
        d.startAngle = std::atan2(screen.y - d.screenOrigin.y, screen.x - d.screenOrigin.x);
        d.sizeComponent = sizeComponentOf(*e);
        if (const auto bounds = localBounds(*e)) {
            d.size = bounds->size;
        }
        m_start = d;
        m_drag = Drag::Transform;
        m_moved = false;
    }

    void dragTo(ui::Vec2 screen, const ui::Modifiers& mods)
    {
        const DragStart& d = m_start;
        const Entity* e = m_editor.document().scene().find(d.id);
        if (e == nullptr) {
            m_drag = Drag::None;
            return;
        }
        const std::string what = e->name().empty() ? std::string("Object") : e->name();
        const Vec2 world = toWorld(screen);
        Vec2 delta = world - d.pointerWorld;
        using C = SetProperties::Change;

        switch (d.handle) {
        case Handle::Body:
        case Handle::MoveXY:
        case Handle::MoveX:
        case Handle::MoveY: {
            if (d.handle == Handle::MoveX) {
                delta.y = 0.0f;
            } else if (d.handle == Handle::MoveY) {
                delta.x = 0.0f;
            }
            Vec2 origin = d.worldOrigin + delta;
            // Whole points; with Shift, the grid.
            const float step = mods.shift ? gridStep(m_zoom) : 1.0f;
            origin = {roundTo(origin.x, step), roundTo(origin.y, step)};
            const Vec2 position = d.parentInverse.apply(origin);
            m_editor.execute(std::make_unique<SetProperties>(
                d.id, std::vector<C>{{"lantern.Transform", "position", position}}, "Move " + what));
            break;
        }
        case Handle::Rotate: {
            const float angle = std::atan2(screen.y - d.screenOrigin.y, screen.x - d.screenOrigin.x);
            // Screen angles grow clockwise (y points down), like Lantern's rotation.
            float degrees = d.rotation + (angle - d.startAngle) * 180.0f / kPi;
            degrees = std::remainder(degrees, 360.0f);
            degrees = mods.shift ? roundTo(degrees, 15.0f) : roundTo(degrees, 0.1f);
            m_editor.execute(std::make_unique<SetProperties>(
                d.id, std::vector<C>{{"lantern.Transform", "rotation", degrees}}, "Rotate " + what));
            break;
        }
        case Handle::ScaleX:
        case Handle::ScaleY:
        case Handle::ScaleXY: {
            const auto g = gizmo();
            if (!g) {
                break;
            }
            const ui::Vec2 fromOrigin{screen.x - d.screenOrigin.x, screen.y - d.screenOrigin.y};
            const ui::Vec2 startFromOrigin{m_pressScreen.x - d.screenOrigin.x, m_pressScreen.y - d.screenOrigin.y};
            Vec2 scale = d.scale;
            const auto along = [](ui::Vec2 v, ui::Vec2 axis) { return v.x * axis.x + v.y * axis.y; };
            if (d.handle == Handle::ScaleXY) {
                const float factor = std::max(0.01f, 1.0f + (screen.x - m_pressScreen.x) / 100.0f);
                scale = {d.scale.x * factor, d.scale.y * factor};
            } else {
                const ui::Vec2 axis = d.handle == Handle::ScaleX ? g->axisX : g->axisY;
                const float start = along(startFromOrigin, axis);
                const float now = along(fromOrigin, axis);
                const float factor = std::abs(start) > 1.0f ? now / start : 1.0f;
                (d.handle == Handle::ScaleX ? scale.x : scale.y) =
                    (d.handle == Handle::ScaleX ? d.scale.x : d.scale.y) * factor;
            }
            const float step = mods.shift ? 0.1f : 0.001f;
            scale = {roundTo(scale.x, step), roundTo(scale.y, step)};
            m_editor.execute(std::make_unique<SetProperties>(
                d.id, std::vector<C>{{"lantern.Transform", "scale", scale}}, "Scale " + what));
            break;
        }
        default: {
            // Rect: move the dragged corner or edge, keep the opposite one in place.
            if (d.sizeComponent == nullptr) {
                break;
            }
            const auto inverse = d.world.inverse();
            if (!inverse) {
                break;
            }
            const Vec2 local = inverse->apply(world);
            float left = -d.size.x * 0.5f;
            float right = d.size.x * 0.5f;
            float bottom = -d.size.y * 0.5f;
            float top = d.size.y * 0.5f;
            const Handle h = d.handle;
            const bool l = h == Handle::RectBL || h == Handle::RectTL || h == Handle::RectL;
            const bool r = h == Handle::RectBR || h == Handle::RectTR || h == Handle::RectR;
            const bool b = h == Handle::RectBL || h == Handle::RectBR || h == Handle::RectB;
            const bool t = h == Handle::RectTL || h == Handle::RectTR || h == Handle::RectT;
            if (l) {
                left = std::min(std::round(local.x), right - 1.0f);
            }
            if (r) {
                right = std::max(std::round(local.x), left + 1.0f);
            }
            if (b) {
                bottom = std::min(std::round(local.y), top - 1.0f);
            }
            if (t) {
                top = std::max(std::round(local.y), bottom + 1.0f);
            }
            Vec2 size{right - left, top - bottom};
            // Shift on a corner keeps the shape's proportions.
            if (mods.shift && (l || r) && (b || t) && d.size.x > 0.0f && d.size.y > 0.0f) {
                const float k = std::max(size.x / d.size.x, size.y / d.size.y);
                size = {std::round(d.size.x * k), std::round(d.size.y * k)};
                if (l) {
                    left = right - size.x;
                } else {
                    right = left + size.x;
                }
                if (b) {
                    bottom = top - size.y;
                } else {
                    top = bottom + size.y;
                }
            }
            const Vec2 centerLocal{(left + right) * 0.5f, (bottom + top) * 0.5f};
            const Vec2 position = d.parentInverse.apply(d.world.apply(centerLocal));
            m_editor.execute(std::make_unique<SetProperties>(
                d.id,
                std::vector<C>{{d.sizeComponent, "size", size}, {"lantern.Transform", "position", position}},
                "Resize " + what));
            break;
        }
        }
    }

    void nudge(Vec2 delta)
    {
        const Scene& scene = m_editor.document().scene();
        const Entity* e = scene.find(m_editor.selection());
        if (e == nullptr) {
            return;
        }
        // In world directions, so arrows move the object on screen as expected.
        const Affine2 parentWorld = e->parent().isNull() ? Affine2{} : scene.worldTransform(e->parent());
        const Affine2 parentInverse = parentWorld.inverse().value_or(Affine2{});
        const Vec2 origin = scene.worldTransform(e->id()).apply({0.0f, 0.0f}) + delta;
        m_editor.execute(std::make_unique<SetProperties>(
            e->id(), std::vector<SetProperties::Change>{{"lantern.Transform", "position", parentInverse.apply(origin)}},
            "Move " + (e->name().empty() ? std::string("Object") : e->name())));
    }

    // --- Painting -----------------------------------------------------------------------------
    void paintGrid(ui::DrawList& draw) const
    {
        const ui::Rect b = bounds();
        const float step = gridStep(m_zoom);
        const Vec2 lo = toWorld({b.x, b.bottom()});
        const Vec2 hi = toWorld({b.right(), b.y});
        const ui::Color minor{255, 255, 255, 10};
        const ui::Color major{255, 255, 255, 22};
        for (float x = std::floor(lo.x / step) * step; x <= hi.x; x += step) {
            const float sx = std::round(toScreen({x, 0.0f}).x) + 0.5f;
            const bool isMajor = std::fmod(std::abs(x), step * 4.0f) < 0.5f;
            draw.line({sx, b.y}, {sx, b.bottom()}, x == 0.0f ? ui::Color{235, 87, 87, 70} : (isMajor ? major : minor));
        }
        for (float y = std::floor(lo.y / step) * step; y <= hi.y; y += step) {
            const float sy = std::round(toScreen({0.0f, y}).y) + 0.5f;
            const bool isMajor = std::fmod(std::abs(y), step * 4.0f) < 0.5f;
            draw.line({b.x, sy}, {b.right(), sy}, y == 0.0f ? ui::Color{110, 200, 90, 70} : (isMajor ? major : minor));
        }
    }

    void paintGameFrame(ui::Context& ctx, ui::DrawList& draw) const
    {
        const auto [min, size] = gameArea();
        const ui::Vec2 a = toScreen(min);
        const ui::Vec2 c = toScreen(min + size);
        const ui::Rect r{std::round(a.x) + 0.5f, std::round(c.y) + 0.5f, std::round(c.x - a.x), std::round(a.y - c.y)};
        draw.strokeRoundedRect(r, 0.0f, ui::Color{255, 255, 255, 110}, 1.0f);
        const ui::Theme& t = ctx.theme();
        draw.text(t.font, t.smallFontSize, {r.x + 4.0f, r.y - 16.0f}, ui::Color{255, 255, 255, 120},
                  m_gameCamera ? "Game view (Camera)" : "Game view");
    }

    void paintSelection(ui::Context& ctx, ui::DrawList& draw) const
    {
        const Scene& scene = m_editor.document().scene();
        const EntityId selected = m_editor.selection();
        if (selected.isNull() || !scene.contains(selected)) {
            return;
        }
        const ui::Theme& t = ctx.theme();
        if (const auto corners = scene.worldCorners(selected)) {
            std::array<ui::Vec2, 4> points{};
            for (std::size_t i = 0; i < 4; ++i) {
                points[i] = toScreen((*corners)[i]);
            }
            draw.polyline(points, ui::Color{0, 0, 0, 120}, 3.5f, true);
            draw.polyline(points, t.accent, 1.5f, true);
        }

        const auto g = gizmo();
        if (!g) {
            return;
        }
        const Handle hot = m_drag == Drag::Transform ? m_start.handle : handleAt(ctx.pointerPosition());
        const ui::Vec2 o = g->origin;
        const ui::Color red = ui::Color::hex(0xEB5757);
        const ui::Color green = ui::Color::hex(0x6FCF4F);
        const ui::Color blue = ui::Color::hex(0x56A8F5);
        const auto tint = [&](ui::Color c, bool active) { return active ? ui::Color::hex(0xFFE08A) : c; };
        const auto end = [&](ui::Vec2 axis) { return ui::Vec2{o.x + axis.x * kGizmoLength, o.y + axis.y * kGizmoLength}; };
        const auto square = [&](ui::Vec2 p, ui::Color c) {
            draw.fillRect({p.x - kHandleSize * 0.5f - 1.0f, p.y - kHandleSize * 0.5f - 1.0f, kHandleSize + 2.0f,
                           kHandleSize + 2.0f},
                          ui::Color{0, 0, 0, 140});
            draw.fillRect({p.x - kHandleSize * 0.5f, p.y - kHandleSize * 0.5f, kHandleSize, kHandleSize}, c);
        };

        switch (m_panel.tool()) {
        case SceneTool::Move: {
            const auto arrow = [&](ui::Vec2 axis, ui::Color c) {
                const ui::Vec2 e = end(axis);
                const ui::Vec2 n{-axis.y, axis.x};
                draw.line(o, e, c, 2.0f);
                const std::array<ui::Vec2, 3> head{
                    ui::Vec2{e.x + axis.x * 12.0f, e.y + axis.y * 12.0f},
                    ui::Vec2{e.x + n.x * 5.0f, e.y + n.y * 5.0f}, ui::Vec2{e.x - n.x * 5.0f, e.y - n.y * 5.0f}};
                // Clockwise on screen for the fill.
                const float cross = (head[1].x - head[0].x) * (head[2].y - head[0].y)
                                    - (head[1].y - head[0].y) * (head[2].x - head[0].x);
                if (cross > 0.0f) {
                    draw.fillConvexPolygon(head, c);
                } else {
                    const std::array<ui::Vec2, 3> rev{head[0], head[2], head[1]};
                    draw.fillConvexPolygon(rev, c);
                }
            };
            arrow(g->axisX, tint(red, hot == Handle::MoveX));
            arrow(g->axisY, tint(green, hot == Handle::MoveY));
            const ui::Color c = tint(blue, hot == Handle::MoveXY || hot == Handle::Body);
            draw.fillRect({o.x + 5.0f, o.y - 19.0f, 14.0f, 14.0f}, c.fade(0.45f));
            draw.strokeRoundedRect({o.x + 5.0f, o.y - 19.0f, 14.0f, 14.0f}, 0.0f, c, 1.5f);
            break;
        }
        case SceneTool::Rotate: {
            std::array<ui::Vec2, 64> ring{};
            for (std::size_t i = 0; i < ring.size(); ++i) {
                const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(ring.size());
                ring[i] = {o.x + std::cos(a) * kGizmoLength, o.y + std::sin(a) * kGizmoLength};
            }
            draw.polyline(ring, ui::Color{0, 0, 0, 120}, 4.0f, true);
            draw.polyline(ring, tint(blue, hot == Handle::Rotate), 2.0f, true);
            const float rot = scene.find(selected)->transform().rotation * kPi / 180.0f;
            draw.line(o, {o.x + std::cos(rot) * kGizmoLength, o.y + std::sin(rot) * kGizmoLength},
                      tint(blue, hot == Handle::Rotate).fade(0.7f), 1.5f);
            break;
        }
        case SceneTool::Scale:
            draw.line(o, end(g->axisX), tint(red, hot == Handle::ScaleX), 2.0f);
            draw.line(o, end(g->axisY), tint(green, hot == Handle::ScaleY), 2.0f);
            square(end(g->axisX), tint(red, hot == Handle::ScaleX));
            square(end(g->axisY), tint(green, hot == Handle::ScaleY));
            square(o, tint(ui::Color::hex(0xDDDDDD), hot == Handle::ScaleXY));
            break;
        case SceneTool::Rect:
            for (const auto& [h, pos] : rectHandles()) {
                square(pos, tint(t.accent, hot == h));
            }
            break;
        }
        if (m_panel.tool() != SceneTool::Scale) {
            draw.fillCircle(o, 3.5f, ui::Color{0, 0, 0, 160});
            draw.fillCircle(o, 2.5f, t.accent);
        }
    }

    EditorServices& m_editor;
    ViewportPanel& m_panel;
    WorldImage m_image;
    float m_worldWidth = 1280.0f;
    float m_worldHeight = 720.0f;
    std::optional<GameCameraView> m_gameCamera;

    Vec2 m_center{640.0f, 360.0f};
    float m_zoom = 0.5f;
    bool m_pendingFit = true;

    Drag m_drag = Drag::None;
    bool m_moved = false;
    ui::Vec2 m_pressScreen;
    Vec2 m_panStartCenter;
    DragStart m_start;
};

// ---------------------------------------------------------------------------
// ViewportPanel
// ---------------------------------------------------------------------------
ViewportPanel::ViewportPanel(EditorServices& editor)
    : ui::Column(0.0f)
    , m_editor(editor)
{
    auto& toolbar = makePanelToolbar(*this);
    const std::array<std::pair<ui::Icon, const char*>, 4> tools{{
        {ui::Icon::Move, "Move tool (W): drag the arrows, the square or the object itself. Shift snaps to the grid."},
        {ui::Icon::Rotate, "Rotate tool (E): drag the circle. Shift turns in 15° steps."},
        {ui::Icon::Scale, "Scale tool (R): drag the squares on the axes, or the middle one for both."},
        {ui::Icon::RectTool, "Rect tool (T): drag a corner or edge to resize shapes and sprites. Shift keeps the "
                             "proportions."},
    }};
    for (std::size_t i = 0; i < tools.size(); ++i) {
        const auto tool = static_cast<SceneTool>(i);
        m_toolButtons[i] = &addToolButton(toolbar, tools[i].first, tools[i].second, [this, tool] { setTool(tool); });
    }
    toolbar.emplace<ui::Separator>(ui::Axis::Vertical).setFixedSize({1.0f, 16.0f});
    addToolButton(toolbar, ui::Icon::Scene, "Show the whole game view", [this] { fitGameView(); });
    toolbar.emplace<ui::Spacer>();
    auto& hint = toolbar.emplace<ui::Label>("Scroll to zoom · right-drag or ⌥-drag to pan", ui::Label::Style::Faint);
    hint.setTooltip("Zoom: scroll wheel or two fingers on a trackpad, around the mouse.\n"
                    "Pan: drag with the right or middle mouse button, or hold ⌥ (Alt) and drag.\n"
                    "F: frame the selected object.");
    m_zoom = &toolbar.emplace<ui::Button>("100%", [this] { openZoomMenu(); });
    m_zoom->setStyle(ui::Button::Style::Subtle);
    m_zoom->setTooltip("Zoom. Click for presets.");

    m_view = &addChild(std::make_unique<SceneView>(editor, *this));
    setTool(SceneTool::Move);
}

void ViewportPanel::setTool(SceneTool tool)
{
    m_tool = tool;
    for (std::size_t i = 0; i < m_toolButtons.size(); ++i) {
        m_toolButtons[i]->setToggled(static_cast<SceneTool>(i) == tool);
    }
}

void ViewportPanel::frameSelection()
{
    m_view->frameSelection();
}

void ViewportPanel::fitGameView()
{
    m_view->fitGameView();
}

void ViewportPanel::openZoomMenu()
{
    std::vector<ui::MenuItem> items;
    const auto add = [&](std::string label, std::string shortcut, std::function<void()> action) {
        ui::MenuItem item;
        item.label = std::move(label);
        item.shortcut = std::move(shortcut);
        item.action = std::move(action);
        items.push_back(std::move(item));
    };
    add("Show the Game View", "", [this] { fitGameView(); });
    add("Frame Selection", "F", [this] { frameSelection(); });
    items.push_back(ui::MenuItem::divider());
    for (float z : {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f}) {
        add(percentText(z), "", [this, z] { m_view->setZoomAround(z, m_view->bounds().center()); });
    }
    openMenuAt(m_editor.ui(), m_zoom->bounds(), std::move(items));
}

std::optional<Vec2> ViewportPanel::worldAt(ui::Vec2 windowPoint) const
{
    if (!isVisible() || !m_view->bounds().contains(windowPoint)) {
        return std::nullopt;
    }
    return m_view->toWorld(windowPoint);
}

void ViewportPanel::update(UiFrame& frame)
{
    m_view->setFrameData(frame.worldImage(), static_cast<float>(frame.worldWidth()),
                         static_cast<float>(frame.worldHeight()), frame.gameCamera());

    // Render the world at this panel's size in real pixels, so it stays sharp.
    const ui::Rect b = m_view->bounds();
    const float pixelScale = frame.ui().atlas().pixelScale();
    if (b.width >= 1.0f && b.height >= 1.0f) {
        frame.setWorldView(WorldView{m_view->center(), m_view->zoom() * pixelScale,
                                     static_cast<int>(std::lround(b.width * pixelScale)),
                                     static_cast<int>(std::lround(b.height * pixelScale))});
    }
    m_zoom->setText(percentText(m_view->zoom()));
}

} // namespace lantern::editor
