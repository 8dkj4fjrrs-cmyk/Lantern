#include "Panels.hpp"

#include "rolo/ui/Context.hpp"

#include <algorithm>
#include <cmath>

namespace rolo::editor {

// The tileset image, cut into its tiles. Click to choose.
class TilePalettePanel::Palette final : public ui::Widget {
public:
    explicit Palette(EditorServices& editor)
        : m_editor(editor)
    {
        setStretch(1.0f);
    }

    void set(AssetId tilesetId, std::optional<Tileset> tileset, ui::TextureId texture, ui::Size imagePixels, bool solidMode)
    {
        m_tilesetId = tilesetId;
        m_tileset = std::move(tileset);
        m_texture = texture;
        m_image = imagePixels;
        m_solidMode = solidMode;
    }

    int tileCount() const
    {
        if (!m_tileset || m_image.width <= 0.0f) {
            return 0;
        }
        return m_tileset->columns(static_cast<int>(m_image.width)) * m_tileset->rows(static_cast<int>(m_image.height));
    }

    void paint(ui::Context& ctx, ui::DrawList& draw) override
    {
        const ui::Theme& t = ctx.theme();
        const ui::Rect b = bounds();
        draw.fillRect(b, ui::Color::hex(0x1B1B1D));
        if (m_texture == 0 || !m_tileset || m_image.width <= 0.0f) {
            return;
        }
        draw.pushClip(b);
        const ui::Rect img = imageRect();
        // A checkerboard behind, so transparent tiles are visible.
        const float check = 8.0f;
        for (float y = img.y; y < img.bottom(); y += check) {
            for (float x = img.x; x < img.right(); x += check) {
                const bool dark = (static_cast<int>((x - img.x) / check) + static_cast<int>((y - img.y) / check)) % 2 == 0;
                draw.fillRect({x, y, std::min(check, img.right() - x), std::min(check, img.bottom() - y)},
                              dark ? ui::Color::hex(0x2A2A2A) : ui::Color::hex(0x333333));
            }
        }
        draw.image(m_texture, img);

        const int imageWidth = static_cast<int>(m_image.width);
        const int count = tileCount();
        const auto hovered = isHovered() ? tileAt(ctx.pointerPosition()) : std::nullopt;
        for (int i = 0; i < count; ++i) {
            const ui::Rect r = tileScreenRect(i, imageWidth);
            draw.strokeRoundedRect(r, 0.0f, ui::Color{0, 0, 0, 70});
            if (m_tileset->isSolid(i)) {
                draw.fillRect(r, ui::Color{235, 64, 64, static_cast<std::uint8_t>(m_solidMode ? 110 : 45)});
                if (m_solidMode) {
                    draw.line({r.x + 3.0f, r.y + 3.0f}, {r.right() - 3.0f, r.bottom() - 3.0f}, ui::Color{255, 255, 255, 160}, 1.5f);
                    draw.line({r.right() - 3.0f, r.y + 3.0f}, {r.x + 3.0f, r.bottom() - 3.0f}, ui::Color{255, 255, 255, 160}, 1.5f);
                }
            }
            if (hovered && *hovered == i) {
                draw.fillRect(r, t.hover);
            }
        }
        if (!m_solidMode) {
            const int selected = m_editor.tileBrush();
            if (selected >= 0 && selected < count) {
                const ui::Rect r = tileScreenRect(selected, imageWidth);
                draw.strokeRoundedRect(r.inset(-1.0f), 0.0f, ui::Color{0, 0, 0, 160}, 3.0f);
                draw.strokeRoundedRect(r.inset(-1.0f), 0.0f, t.accent, 2.0f);
            }
        }
        draw.popClip();
    }

    bool onPointerDown(ui::Context&, const ui::PointerEvent& event) override
    {
        if (event.button != ui::MouseButton::Left) {
            return false;
        }
        const auto tile = tileAt(event.position);
        if (!tile) {
            return true;
        }
        if (m_solidMode) {
            toggleSolid(*tile);
        } else {
            m_editor.setTileBrush(*tile);
            if (m_editor.tileLayer() == PaintTiles::kCollisionLayer) {
                m_editor.setTileLayer(0); // choosing a tile means painting tiles
            }
            m_editor.setSceneTool(static_cast<int>(SceneTool::Brush));
        }
        return true;
    }

    std::string tooltip() const override
    {
        return m_solidMode ? "Click tiles to make them solid (they block movement) or not"
                           : "Click a tile to paint with it in the Scene view";
    }

private:
    // The image, scaled to fit: whole-number zoom for crisp pixel art.
    ui::Rect imageRect() const
    {
        const ui::Rect b = bounds().inset(8.0f);
        float scale = std::min(b.width / m_image.width, b.height / m_image.height);
        if (scale >= 1.0f) {
            scale = std::min(8.0f, std::floor(scale));
        }
        const float w = std::round(m_image.width * scale);
        const float h = std::round(m_image.height * scale);
        return {std::round(b.x), std::round(b.y), w, h};
    }

    ui::Rect tileScreenRect(int index, int imageWidth) const
    {
        const ui::Rect img = imageRect();
        const float s = img.width / m_image.width;
        const TileRect r = m_tileset->tileRect(index, imageWidth);
        return {img.x + static_cast<float>(r.x) * s, img.y + static_cast<float>(r.y) * s,
                static_cast<float>(r.width) * s, static_cast<float>(r.height) * s};
    }

    std::optional<int> tileAt(ui::Vec2 p) const
    {
        if (!m_tileset || m_image.width <= 0.0f) {
            return std::nullopt;
        }
        const int imageWidth = static_cast<int>(m_image.width);
        for (int i = 0; i < tileCount(); ++i) {
            if (tileScreenRect(i, imageWidth).contains(p)) {
                return i;
            }
        }
        return std::nullopt;
    }

    void toggleSolid(int tile)
    {
        AssetDatabase* db = m_editor.assets();
        const AssetRecord* asset = db != nullptr ? db->find(m_tilesetId) : nullptr;
        if (asset == nullptr || !asset->tileset) {
            return;
        }
        Tileset changed = *asset->tileset;
        const bool solid = changed.solid.erase(tile) == 0;
        if (solid) {
            changed.solid.insert(tile);
        }
        // Saved to the tileset file straight away, and undoable like any edit.
        m_editor.endEdit();
        if (!m_editor.execute(std::make_unique<ChangeTileset>(m_tilesetId, std::move(changed),
                                                              solid ? "Make Tile Solid" : "Make Tile Not Solid"))) {
            m_editor.showMessage("Could Not Save the Tileset",
                                 "Rolo could not write " + asset->path + ". Check that the file is not read-only.");
        }
    }

    EditorServices& m_editor;
    AssetId m_tilesetId;
    std::optional<Tileset> m_tileset;
    ui::TextureId m_texture = 0;
    ui::Size m_image;
    bool m_solidMode = false;
};

TilePalettePanel::TilePalettePanel(EditorServices& editor)
    : ui::Column(0.0f)
    , m_editor(editor)
{
    m_empty = &emplace<ui::EmptyState>(ui::Icon::Grid, "No tilemap selected",
                                       "Select a tilemap to paint it (or create one: Hierarchy + > Tilemap). A "
                                       "tilemap needs a tileset: select an image in Assets and choose Create Tileset.");
    auto body = std::make_unique<ui::Column>(0.0f);
    body->setStretch(1.0f);
    auto& toolbar = makePanelToolbar(*body);
    m_info = &toolbar.emplace<ui::Label>("", ui::Label::Style::Dim);
    m_info->setStretch(1.0f);
    m_solidMode = &toolbar.emplace<ui::Checkbox>("Edit solid tiles", false);
    m_solidMode->setTooltip("Solid tiles block movement wherever they are painted. Saved in the tileset, so every "
                            "tilemap using it agrees.");
    m_palette = &body->addChild(std::make_unique<Palette>(editor));
    m_body = &addChild(std::move(body));
}

void TilePalettePanel::update(UiFrame& frame)
{
    const Scene& scene = m_editor.document().scene();
    const Entity* e = scene.find(m_editor.selection());
    const Tilemap* map = e != nullptr ? e->get<Tilemap>() : nullptr;
    const AssetDatabase* db = m_editor.assets();
    const AssetRecord* ts = db != nullptr && map != nullptr ? db->find(map->tileset) : nullptr;
    const AssetRecord* img = ts != nullptr && ts->tileset ? db->find(ts->tileset->image) : nullptr;

    m_empty->setVisible(map == nullptr);
    m_body->setVisible(map != nullptr);
    if (map == nullptr) {
        return;
    }
    if (ts == nullptr || !ts->tileset) {
        m_info->setText(map->tileset.isNull() ? "Choose a Tileset in the Inspector" : "The tileset is missing");
        m_palette->set({}, std::nullopt, 0, {}, false);
        return;
    }
    ui::TextureId texture = 0;
    ui::Size size;
    if (img != nullptr && img->isUsable()) {
        if (const auto loaded = frame.loadImage(db->absolutePath(*img), img->filter == TextureFilter::Pixel)) {
            texture = loaded->texture;
            size = loaded->size;
        }
    }
    m_palette->set(ts->id, ts->tileset, texture, size, m_solidMode->isChecked());
    const int layer = m_editor.tileLayer();
    const std::string target = layer == PaintTiles::kCollisionLayer ? "Collision"
                               : layer >= 0 && static_cast<std::size_t>(layer) < map->layers.size()
                                   ? map->layers[static_cast<std::size_t>(layer)].name
                                   : "";
    m_info->setText(ts->fileName() + "   ·   tile " + std::to_string(m_editor.tileBrush()) + " of "
                    + std::to_string(m_palette->tileCount()) + "   ·   painting on " + target);
}

} // namespace rolo::editor
