// The Animation panel: edits an animation set (.lanim).
//
// Layout: a toolbar to choose, add, rename and delete clips; the sprite sheet
// with its frames (click one to add it to the clip); the clip's frames in
// order; and on the right a preview playing the clip, with its speed and
// looping. Every change is saved to the file at once as an undoable step
// (editAnimationSet), so Undo and Redo work as for scene edits.

#include "Panels.hpp"

#include "rolo/ui/Context.hpp"
#include "rolo/ui/widgets/Dialog.hpp"

#include <algorithm>
#include <cmath>

namespace rolo::editor {

namespace {

constexpr float kCell = 56.0f;   // a frame in the strip
constexpr float kGap = 6.0f;
constexpr float kPad = 8.0f;

// A checkerboard, so transparent pixels are visible.
void drawChecker(ui::DrawList& draw, ui::Rect r, float check = 8.0f)
{
    for (float y = r.y; y < r.bottom(); y += check) {
        for (float x = r.x; x < r.right(); x += check) {
            const bool dark = (static_cast<int>((x - r.x) / check) + static_cast<int>((y - r.y) / check)) % 2 == 0;
            draw.fillRect({x, y, std::min(check, r.right() - x), std::min(check, r.bottom() - y)},
                          dark ? ui::Color::hex(0x2A2A2A) : ui::Color::hex(0x333333));
        }
    }
}

// The largest rectangle of the frame's shape inside `box`, centred; whole-
// number zoom when enlarging, so pixel art stays crisp.
ui::Rect fitFrame(ui::Rect box, float w, float h)
{
    if (w <= 0.0f || h <= 0.0f) {
        return box;
    }
    float scale = std::min(box.width / w, box.height / h);
    if (scale >= 1.0f) {
        scale = std::floor(scale);
    }
    const float fw = std::round(w * scale);
    const float fh = std::round(h * scale);
    return {std::round(box.x + (box.width - fw) * 0.5f), std::round(box.y + (box.height - fh) * 0.5f), fw, fh};
}

std::string uniqueClipId(const AnimationSet& set, const std::string& base)
{
    std::string id = base;
    for (int n = 2; set.findClip(id) != nullptr; ++n) {
        id = base + "_" + std::to_string(n);
    }
    return id;
}

} // namespace

int AnimationPanel::State::frameCount() const
{
    if (!set || image.width <= 0.0f) {
        return 0;
    }
    return set->columns(static_cast<int>(image.width)) * set->rows(static_cast<int>(image.height));
}

std::pair<ui::Vec2, ui::Vec2> AnimationPanel::State::frameUv(int frame) const
{
    const TileRect r = set->frameRect(frame, static_cast<int>(image.width));
    return {{static_cast<float>(r.x) / image.width, static_cast<float>(r.y) / image.height},
            {static_cast<float>(r.x + r.width) / image.width, static_cast<float>(r.y + r.height) / image.height}};
}

// --- The sprite sheet ------------------------------------------------------------------------------

class AnimationPanel::Sheet final : public ui::Widget {
public:
    Sheet(State& state, std::function<void(int frame, bool range)> onPick)
        : m_state(state)
        , m_onPick(std::move(onPick))
    {
        setStretch(1.0f);
    }

    void paint(ui::Context& ctx, ui::DrawList& draw) override
    {
        const ui::Theme& t = ctx.theme();
        const ui::Rect b = bounds();
        draw.fillRect(b, ui::Color::hex(0x1B1B1D));
        const int count = m_state.frameCount();
        if (m_state.texture == 0 || count == 0) {
            const char* why = m_state.texture == 0 ? "The sheet's image is missing (choose one in the Inspector)"
                                                   : "No frames: the frame size is larger than the image";
            draw.text(t.font, t.fontSize, {b.x + 12.0f, b.y + 12.0f}, t.textDim, why);
            return;
        }
        draw.pushClip(b);
        const ui::Rect img = imageRect();
        drawChecker(draw, img);
        draw.image(m_state.texture, img);

        // Frames used by the clip are tinted, with how often they appear.
        const AnimationClip* clip = m_state.currentClip();
        const auto hovered = isHovered() ? frameAt(ctx.pointerPosition()) : std::nullopt;
        for (int i = 0; i < count; ++i) {
            const ui::Rect r = frameScreenRect(i);
            draw.strokeRoundedRect(r, 0.0f, ui::Color{0, 0, 0, 90});
            const auto uses = clip != nullptr ? std::count(clip->frames.begin(), clip->frames.end(), i) : 0;
            if (uses > 0) {
                draw.fillRect(r, ui::Color::hex(0xF2A93B, 40));
                draw.strokeRoundedRect(r.inset(1.0f), 0.0f, ui::Color::hex(0xF2A93B, 160));
            }
            if (r.width >= 26.0f) {
                draw.text(t.font, t.smallFontSize, {r.x + 3.0f, r.y + 2.0f}, ui::Color{255, 255, 255, 150},
                          std::to_string(i));
            }
            if (hovered && *hovered == i) {
                draw.fillRect(r, t.hover);
            }
        }
        // The frame selected in the strip.
        if (clip != nullptr && m_state.selected >= 0 && static_cast<std::size_t>(m_state.selected) < clip->frames.size()) {
            const int frame = clip->frames[static_cast<std::size_t>(m_state.selected)];
            if (frame >= 0 && frame < count) {
                const ui::Rect r = frameScreenRect(frame);
                draw.strokeRoundedRect(r.inset(-1.0f), 0.0f, ui::Color{0, 0, 0, 160}, 3.0f);
                draw.strokeRoundedRect(r.inset(-1.0f), 0.0f, t.accent, 2.0f);
            }
        }
        draw.popClip();
    }

    bool onPointerDown(ui::Context& ctx, const ui::PointerEvent& event) override
    {
        if (event.button != ui::MouseButton::Left) {
            return false;
        }
        if (const auto frame = frameAt(event.position)) {
            m_onPick(*frame, ctx.modifiers().shift);
        }
        return true;
    }

    std::string tooltip() const override
    {
        return "Click a frame to add it to the clip (after the selected frame). Shift-click adds every frame "
               "from the last one up to the one clicked.";
    }

private:
    ui::Rect imageRect() const
    {
        return fitFrame(bounds().inset(kPad), m_state.image.width, m_state.image.height);
    }

    ui::Rect frameScreenRect(int index) const
    {
        const ui::Rect img = imageRect();
        const float s = img.width / m_state.image.width;
        const TileRect r = m_state.set->frameRect(index, static_cast<int>(m_state.image.width));
        return {img.x + static_cast<float>(r.x) * s, img.y + static_cast<float>(r.y) * s,
                static_cast<float>(r.width) * s, static_cast<float>(r.height) * s};
    }

    std::optional<int> frameAt(ui::Vec2 p) const
    {
        const int count = m_state.frameCount();
        for (int i = 0; i < count; ++i) {
            if (frameScreenRect(i).contains(p)) {
                return i;
            }
        }
        return std::nullopt;
    }

    State& m_state;
    std::function<void(int, bool)> m_onPick;
};

// --- The clip's frames, in order -----------------------------------------------------------------------

class AnimationPanel::Strip final : public ui::Widget {
public:
    explicit Strip(State& state)
        : m_state(state)
    {
        setFocusable(true);
    }

    std::function<void()> onRemove;
    std::function<void(int)> onMove;

    ui::Size measure(ui::Context&, ui::Size available) override
    {
        const AnimationClip* clip = m_state.currentClip();
        const int count = clip != nullptr ? static_cast<int>(clip->frames.size()) : 0;
        const int rows = (std::max(1, count) + perRow(available.width) - 1) / perRow(available.width);
        return {available.width, kPad * 2.0f + static_cast<float>(rows) * (kCell + kGap) - kGap};
    }

    void paint(ui::Context& ctx, ui::DrawList& draw) override
    {
        const ui::Theme& t = ctx.theme();
        const ui::Rect b = bounds();
        draw.fillRect(b, ui::Color::hex(0x202022));
        const AnimationClip* clip = m_state.currentClip();
        if (clip == nullptr || clip->frames.empty()) {
            draw.text(t.font, t.fontSize, {b.x + 12.0f, b.y + kPad + 18.0f}, t.textFaint,
                      clip == nullptr ? "Add a clip to start (+ in the toolbar)" : "No frames yet: click frames in the sheet above");
            return;
        }
        const int count = m_state.frameCount();
        const auto hovered = isHovered() ? cellAt(ctx.pointerPosition()) : std::nullopt;
        for (std::size_t i = 0; i < clip->frames.size(); ++i) {
            const ui::Rect cell = cellRect(static_cast<int>(i));
            draw.fillRect(cell, ui::Color::hex(0x161616));
            const int frame = clip->frames[i];
            if (frame >= 0 && frame < count && m_state.texture != 0) {
                const ui::Rect pic = fitFrame(cell.inset(4.0f), static_cast<float>(m_state.set->frameWidth),
                                              static_cast<float>(m_state.set->frameHeight));
                drawChecker(draw, pic, 4.0f);
                const auto [uv0, uv1] = m_state.frameUv(frame);
                draw.image(m_state.texture, pic, uv0, uv1);
            } else {
                draw.text(t.font, t.fontSize, {cell.x + 10.0f, cell.y + 20.0f}, t.error, "?");
            }
            draw.text(t.font, t.smallFontSize, {cell.x + 3.0f, cell.y + 2.0f}, ui::Color{255, 255, 255, 170},
                      std::to_string(frame));
            if (hovered && *hovered == static_cast<int>(i)) {
                draw.fillRect(cell, t.hover);
            }
            if (static_cast<int>(i) == m_state.selected) {
                draw.strokeRoundedRect(cell, 2.0f, t.accent, 2.0f);
            } else {
                draw.strokeRoundedRect(cell, 2.0f, t.border);
            }
        }
    }

    bool onPointerDown(ui::Context& ctx, const ui::PointerEvent& event) override
    {
        if (event.button != ui::MouseButton::Left) {
            return false;
        }
        ctx.setFocus(this);
        m_state.selected = cellAt(event.position).value_or(-1);
        return true;
    }

    bool onKeyDown(ui::Context&, const ui::KeyEvent& event) override
    {
        const AnimationClip* clip = m_state.currentClip();
        if (clip == nullptr || m_state.selected < 0) {
            return false;
        }
        const int last = static_cast<int>(clip->frames.size()) - 1;
        switch (event.key) {
        case ui::Key::Delete:
        case ui::Key::Backspace:
            if (onRemove) {
                onRemove();
            }
            return true;
        case ui::Key::Left:
            if (event.modifiers.alt && onMove) {
                onMove(-1);
            } else {
                m_state.selected = std::max(0, m_state.selected - 1);
            }
            return true;
        case ui::Key::Right:
            if (event.modifiers.alt && onMove) {
                onMove(1);
            } else {
                m_state.selected = std::min(last, m_state.selected + 1);
            }
            return true;
        default:
            return false;
        }
    }

    std::string tooltip() const override
    {
        return "The clip's frames, in the order they play. Click one to select it; Delete removes it, Alt+Left / "
               "Alt+Right moves it.";
    }

private:
    static int perRow(float width)
    {
        return std::max(1, static_cast<int>((width - kPad * 2.0f + kGap) / (kCell + kGap)));
    }
    ui::Rect cellRect(int index) const
    {
        const int n = perRow(bounds().width);
        const ui::Rect b = bounds();
        return {b.x + kPad + static_cast<float>(index % n) * (kCell + kGap),
                b.y + kPad + static_cast<float>(index / n) * (kCell + kGap), kCell, kCell};
    }
    std::optional<int> cellAt(ui::Vec2 p) const
    {
        const AnimationClip* clip = m_state.currentClip();
        for (int i = 0; clip != nullptr && i < static_cast<int>(clip->frames.size()); ++i) {
            if (cellRect(i).contains(p)) {
                return i;
            }
        }
        return std::nullopt;
    }

    State& m_state;
};

// --- The preview ------------------------------------------------------------------------------------------

class AnimationPanel::Preview final : public ui::Widget {
public:
    explicit Preview(State& state)
        : m_state(state)
    {
    }

    void paint(ui::Context& ctx, ui::DrawList& draw) override
    {
        const ui::Theme& t = ctx.theme();
        const ui::Rect b = bounds();
        draw.fillRect(b, ui::Color::hex(0x161616));
        const AnimationClip* clip = m_state.currentClip();
        if (clip == nullptr || clip->frames.empty() || m_state.texture == 0) {
            return;
        }
        // Start again from the first frame whenever the clip changes.
        if (clip->id != m_clipShown || clip->frames != m_framesShown) {
            m_clipShown = clip->id;
            m_framesShown = clip->frames;
            m_start = ctx.time();
        }
        int frame = 0;
        int index = 0;
        if (m_state.previewPlaying) {
            double elapsed = ctx.time() - m_start;
            if (!clip->loop) {
                // Play once, hold the last frame a moment, then again.
                elapsed = std::fmod(elapsed, clip->duration() + 0.6);
            }
            frame = clip->frameAt(elapsed);
            index = clip->fps > 0.0f ? static_cast<int>(elapsed * static_cast<double>(clip->fps)) : 0;
            index = clip->loop ? index % static_cast<int>(clip->frames.size())
                               : std::min(index, static_cast<int>(clip->frames.size()) - 1);
        } else {
            index = std::clamp(m_state.selected, 0, static_cast<int>(clip->frames.size()) - 1);
            frame = clip->frames[static_cast<std::size_t>(index)];
        }
        if (frame < 0 || frame >= m_state.frameCount()) {
            return;
        }
        const ui::Rect pic = fitFrame(b.inset(10.0f), static_cast<float>(m_state.set->frameWidth),
                                      static_cast<float>(m_state.set->frameHeight));
        drawChecker(draw, pic);
        const auto [uv0, uv1] = m_state.frameUv(frame);
        draw.image(m_state.texture, pic, uv0, uv1);
        draw.text(t.font, t.smallFontSize, {b.x + 6.0f, b.bottom() - 16.0f}, t.textFaint,
                  std::to_string(index + 1) + " / " + std::to_string(clip->frames.size()));
    }

private:
    State& m_state;
    std::string m_clipShown;
    std::vector<int> m_framesShown;
    double m_start = 0.0;
};

// --- The panel --------------------------------------------------------------------------------------------

AnimationPanel::AnimationPanel(EditorServices& editor)
    : ui::Column(0.0f)
    , m_editor(editor)
{
    m_empty = &emplace<ui::EmptyState>(ui::Icon::Play, "No animations selected",
                                       "Select an animation file (.lanim) in Assets, or an object with an Animator. To "
                                       "make one, select a sprite sheet image and choose Create Animations.");
    auto body = std::make_unique<ui::Column>(0.0f);
    body->setStretch(1.0f);

    // Toolbar: the clip, and clip actions.
    auto& toolbar = makePanelToolbar(*body);
    m_info = &toolbar.emplace<ui::Label>("", ui::Label::Style::Dim);
    m_info->setStretch(1.0f);
    toolbar.emplace<ui::Label>("Clip", ui::Label::Style::Dim);
    m_clipChoice = &toolbar.emplace<ui::ChoiceField>();
    m_clipChoice->setFixedSize({150.0f, 24.0f});
    m_clipChoice->setStretch(0.0f);
    m_clipChoice->setTooltip("The clip being edited");
    m_clipChoice->setOnChoose([this](int index) {
        if (m_state.set && index >= 0 && static_cast<std::size_t>(index) < m_state.set->clips.size()) {
            m_state.clip = m_state.set->clips[static_cast<std::size_t>(index)].id;
            m_state.selected = -1;
            refreshControls();
        }
    });
    addToolButton(toolbar, ui::Icon::Plus, "New clip", [this] { showClipNameDialog(false); });
    m_clipButtons.push_back(&addToolButton(toolbar, ui::Icon::RectTool, "Rename clip", [this] { showClipNameDialog(true); }));
    m_clipButtons.push_back(&addToolButton(toolbar, ui::Icon::Close, "Delete clip", [this] { deleteClip(); }));
    toolbar.emplace<ui::Separator>(ui::Axis::Vertical);
    toolbar.emplace<ui::Label>("Speed", ui::Label::Style::Dim);
    m_fps = &toolbar.emplace<ui::NumberField>();
    m_fps->setFixedSize({84.0f, 24.0f});
    m_fps->setStretch(0.0f);
    m_fps->setRange(0.1f, 240.0f);
    m_fps->setStep(1.0f);
    m_fps->setSuffix("fps");
    m_fps->setTooltip("Frames shown per second");
    m_fps->setOnValue([this](float v) {
        editClip("Change Clip Speed", [v](AnimationClip& c) { c.fps = std::clamp(v, 0.1f, 240.0f); }, true);
    });
    m_fps->setOnEditEnd([this] { m_editor.endEdit(); });
    m_loop = &toolbar.emplace<ui::Checkbox>("Loop", true);
    m_loop->setTooltip("Start again after the last frame. Off: stays on the last frame (e.g. an attack), and the "
                       "game can notice it finished.");
    m_loop->setOnChange([this](bool on) { editClip(on ? "Loop Clip" : "Play Clip Once", [on](AnimationClip& c) { c.loop = on; }); });

    auto& main = body->emplace<ui::Row>(0.0f);
    main.setStretch(1.0f);

    // Left: the sheet over the strip.
    auto& left = main.emplace<ui::Column>(0.0f);
    left.setStretch(1.0f);
    left.addChild(std::make_unique<Sheet>(m_state, [this](int frame, bool range) {
        const AnimationClip* clip = m_state.currentClip();
        if (clip == nullptr) {
            m_editor.showMessage("No Clip", "Add a clip first (+ in the Animation panel's toolbar).");
            return;
        }
        // Where new frames go: after the selected one, or at the end.
        const int at = m_state.selected >= 0 ? std::min(m_state.selected + 1, static_cast<int>(clip->frames.size()))
                                             : static_cast<int>(clip->frames.size());
        std::vector<int> added{frame};
        if (range) {
            const int before = at > 0 ? clip->frames[static_cast<std::size_t>(at - 1)] : -1;
            if (before >= 0 && before != frame) {
                added.clear();
                const int step = frame > before ? 1 : -1;
                for (int f = before + step; f != frame + step; f += step) {
                    added.push_back(f);
                }
            }
        }
        editClip(added.size() == 1 ? "Add Frame" : "Add Frames", [&](AnimationClip& c) {
            c.frames.insert(c.frames.begin() + at, added.begin(), added.end());
        });
        m_state.selected = at + static_cast<int>(added.size()) - 1;
    }));
    auto& stripHeader = left.emplace<ui::Row>(6.0f, ui::Insets{8.0f, 4.0f, 8.0f, 2.0f});
    stripHeader.setCrossAlign(ui::CrossAlign::Center);
    m_frameInfo = &stripHeader.emplace<ui::Label>("Frames", ui::Label::Style::Dim);
    m_frameInfo->setStretch(1.0f);
    m_frameButtons.push_back(&addToolButton(stripHeader, ui::Icon::ChevronLeft, "Move the frame earlier (Alt+Left)",
                                            [this] { moveSelected(-1); }));
    m_frameButtons.push_back(&addToolButton(stripHeader, ui::Icon::ChevronRight, "Move the frame later (Alt+Right)",
                                            [this] { moveSelected(1); }));
    m_frameButtons.push_back(&addToolButton(stripHeader, ui::Icon::Eraser, "Remove the frame (Delete)",
                                            [this] { removeSelected(); }));
    auto& strip = left.addChild(std::make_unique<Strip>(m_state));
    strip.onRemove = [this] { removeSelected(); };
    strip.onMove = [this](int by) { moveSelected(by); };

    // Right: the preview, with play / pause.
    main.emplace<ui::Separator>(ui::Axis::Vertical);
    auto& right = main.emplace<ui::Column>(6.0f, ui::Insets{8.0f, 8.0f, 8.0f, 8.0f});
    right.setFixedSize({200.0f, 0.0f});
    right.addChild(std::make_unique<Preview>(m_state)).setStretch(1.0f);
    m_play = &right.emplace<ui::Button>("Pause");
    m_play->setIcon(ui::Icon::Pause);
    m_play->setTooltip("Paused, the preview shows the selected frame. Gameplay code plays clips by name, e.g. "
                       "game.play(entity(), \"walk_left\").");
    m_play->setOnClick([this] {
        m_state.previewPlaying = !m_state.previewPlaying;
        refreshControls();
    });

    m_body = &addChild(std::move(body));
    refreshControls();
}

void AnimationPanel::showSet(AssetId id, const std::string& clip)
{
    if (id != m_state.setId) {
        m_state.setId = id;
        m_state.selected = -1;
        m_shownRevision = 0;   // read it again below
    }
    if (!clip.empty()) {
        if (clip != m_state.clip) {
            m_state.selected = -1;
        }
        m_state.clip = clip;
    }
}

void AnimationPanel::update(UiFrame& frame)
{
    AssetDatabase* db = m_editor.assets();

    // Follow the selection: an animation file in Assets, or an object's Animator.
    if (m_editor.selectedAsset() != m_lastSelectedAsset) {
        m_lastSelectedAsset = m_editor.selectedAsset();
        const AssetRecord* a = db != nullptr ? db->find(m_lastSelectedAsset) : nullptr;
        if (a != nullptr && a->type == AssetType::Animation) {
            showSet(a->id, {});
        }
    }
    if (m_editor.selection() != m_lastSelectedEntity) {
        m_lastSelectedEntity = m_editor.selection();
        const Entity* e = m_editor.document().scene().find(m_lastSelectedEntity);
        if (const Animator* animator = e != nullptr ? e->get<Animator>() : nullptr; animator && !animator->animations.isNull()) {
            showSet(animator->animations, animator->clip);
        }
    }

    const AssetRecord* asset = db != nullptr && !m_state.setId.isNull() ? db->find(m_state.setId) : nullptr;
    if (asset == nullptr || asset->type != AssetType::Animation) {
        m_state.setId = {};
        m_state.set.reset();
        m_empty->setVisible(true);
        m_body->setVisible(false);
        return;
    }
    m_empty->setVisible(false);
    m_body->setVisible(true);

    // The file changed (an edit, undo, or another program): read it again.
    if (db->revision() != m_shownRevision) {
        m_shownRevision = db->revision();
        m_state.set = asset->animation;
        if (m_state.set && m_state.set->findClip(m_state.clip) == nullptr) {
            m_state.clip = m_state.set->clips.empty() ? std::string() : m_state.set->clips.front().id;
            m_state.selected = -1;
        }
        refreshControls();
    }

    m_state.texture = 0;
    m_state.image = {};
    const AssetRecord* image = m_state.set ? db->find(m_state.set->image) : nullptr;
    if (image != nullptr && image->isUsable()) {
        if (const auto loaded = frame.loadImage(db->absolutePath(*image), image->filter == TextureFilter::Pixel)) {
            m_state.texture = loaded->texture;
            m_state.image = loaded->size;
        }
    }
    std::string info = asset->fileName();
    if (!m_state.set) {
        info += "   ·   cannot be read: " + asset->problem;
    } else {
        info += "   ·   " + std::to_string(m_state.set->frameWidth) + " × " + std::to_string(m_state.set->frameHeight)
                + " px frames";
    }
    m_info->setText(info);

    const AnimationClip* clip = m_state.currentClip();
    if (clip != nullptr && m_state.selected >= static_cast<int>(clip->frames.size())) {
        m_state.selected = static_cast<int>(clip->frames.size()) - 1;
    }
    const bool hasFrame = clip != nullptr && m_state.selected >= 0;
    for (ui::Button* b : m_frameButtons) {
        b->setEnabled(hasFrame);
    }
    m_frameInfo->setText(clip == nullptr ? "Frames"
                                         : "Frames of " + clip->id + "  ·  " + std::to_string(clip->frames.size())
                                               + "  ·  " + std::to_string(clip->duration()).substr(0, 4) + " s");
}

void AnimationPanel::refreshControls()
{
    std::vector<std::string> names;
    int selected = -1;
    if (m_state.set) {
        for (std::size_t i = 0; i < m_state.set->clips.size(); ++i) {
            names.push_back(m_state.set->clips[i].id);
            if (m_state.set->clips[i].id == m_state.clip) {
                selected = static_cast<int>(i);
            }
        }
    }
    if (names.empty()) {
        names.push_back("No clips");
        selected = 0;
    }
    m_clipChoice->setOptions(std::move(names));
    m_clipChoice->setSelected(selected);
    const AnimationClip* clip = m_state.currentClip();
    m_clipChoice->setEnabled(clip != nullptr);
    for (ui::Button* b : m_clipButtons) {
        b->setEnabled(clip != nullptr);
    }
    m_fps->setEnabled(clip != nullptr);
    m_loop->setEnabled(clip != nullptr);
    if (clip != nullptr) {
        if (!m_fps->isEditing()) {
            m_fps->setValue(clip->fps);
        }
        m_loop->setChecked(clip->loop);
    }
    m_play->setText(m_state.previewPlaying ? "Pause" : "Play");
    m_play->setIcon(m_state.previewPlaying ? ui::Icon::Pause : ui::Icon::Play);
}

void AnimationPanel::editClip(const std::string& label, const std::function<void(AnimationClip&)>& change, bool merge)
{
    const std::string clipId = m_state.clip;
    editAnimationSet(
        m_editor, m_state.setId, label,
        [&](AnimationSet& set) {
            for (AnimationClip& c : set.clips) {
                if (c.id == clipId) {
                    change(c);
                }
            }
        },
        merge);
}

void AnimationPanel::moveSelected(int by)
{
    const AnimationClip* clip = m_state.currentClip();
    const int from = m_state.selected;
    const int to = from + by;
    if (clip == nullptr || from < 0 || to < 0 || to >= static_cast<int>(clip->frames.size())) {
        return;
    }
    editClip("Move Frame", [from, to](AnimationClip& c) {
        std::swap(c.frames[static_cast<std::size_t>(from)], c.frames[static_cast<std::size_t>(to)]);
    });
    m_state.selected = to;
}

void AnimationPanel::removeSelected()
{
    const AnimationClip* clip = m_state.currentClip();
    const int at = m_state.selected;
    if (clip == nullptr || at < 0 || at >= static_cast<int>(clip->frames.size())) {
        return;
    }
    editClip("Remove Frame", [at](AnimationClip& c) { c.frames.erase(c.frames.begin() + at); });
    m_state.selected = std::min(at, static_cast<int>(clip->frames.size()) - 2);
}

void AnimationPanel::deleteClip()
{
    const std::string id = m_state.clip;
    editAnimationSet(m_editor, m_state.setId, "Delete Clip", [&](AnimationSet& set) {
        std::erase_if(set.clips, [&](const AnimationClip& c) { return c.id == id; });
    });
}

void AnimationPanel::showClipNameDialog(bool rename)
{
    if (!m_state.set || (rename && m_state.currentClip() == nullptr)) {
        return;
    }
    auto dialog = std::make_unique<ui::Dialog>(rename ? "Rename Clip" : "New Clip", 380.0f);
    ui::Column& body = dialog->body();
    auto& nameRow = body.emplace<ui::PropertyField>("Name");
    auto& name = nameRow.addValue(std::make_unique<ui::TextField>(""));
    name.setStretch(1.0f);
    name.setFixedSize({0.0f, 24.0f});
    name.setSelectAllOnFocus(true);
    name.setText(rename ? m_state.clip : uniqueClipId(*m_state.set, "walk"));
    auto& note = body.emplace<ui::Label>(rename ? "Animators and gameplay code that use the old name need the new "
                                                  "one. Letters, digits and _."
                                                : "e.g. idle, walk_left, attack. Letters, digits and _.",
                                         ui::Label::Style::Faint);
    note.setWrap(true);
    dialog->addButton("Cancel");
    const std::string old = m_state.clip;
    dialog->addButton(rename ? "Rename" : "Create", ui::Button::Style::Primary, [this, rename, old, &name] {
        const std::string text = name.text();
        if (!isValidClipId(text)) {
            m_editor.showMessage("Invalid Name",
                                 "A clip name uses letters, digits and _, and starts with a letter or _ (e.g. walk_left).");
            return;
        }
        if (text == old && rename) {
            return;
        }
        if (m_state.set && m_state.set->findClip(text) != nullptr) {
            m_editor.showMessage("Name Taken", "There is already a clip called " + text + ".");
            return;
        }
        const bool done = editAnimationSet(m_editor, m_state.setId, rename ? "Rename Clip" : "Add Clip",
                                           [&](AnimationSet& set) {
                                               if (!rename) {
                                                   set.clips.push_back(AnimationClip{text, {}, 8.0f, true});
                                                   return;
                                               }
                                               for (AnimationClip& c : set.clips) {
                                                   if (c.id == old) {
                                                       c.id = text;
                                                   }
                                               }
                                           });
        if (done) {
            m_state.clip = text;
            m_state.selected = -1;
        }
    });
    ui::showDialog(m_editor.ui(), std::move(dialog));
    m_editor.ui().setFocus(&name);
}

} // namespace rolo::editor
