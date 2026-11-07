#include "TextureStore.hpp"

namespace rolo::detail {

namespace {
constexpr std::string_view kCategory = "runtime";

void applyFilter(ax::Texture2D& texture, bool pixelArt)
{
    if (pixelArt) {
        texture.setAliasTexParameters();     // nearest pixel: crisp pixel art
    } else {
        texture.setAntiAliasTexParameters(); // linear: smooth scaling
    }
}
} // namespace

TextureStore::TextureStore(const Logger& log)
    : m_log(log)
{
}

TextureStore::~TextureStore()
{
    clear();
}

void TextureStore::clear()
{
    for (auto& [path, entry] : m_entries) {
        AX_SAFE_RELEASE_NULL(entry.texture);
    }
    m_entries.clear();
    ++m_generation;
}

ax::Texture2D* TextureStore::get(const std::filesystem::path& path, bool pixelArt)
{
    std::error_code ec;
    const auto stamp = std::filesystem::last_write_time(path, ec);
    const std::string key = path.string();
    auto it = m_entries.find(key);
    if (it != m_entries.end() && !ec && it->second.stamp == stamp) {
        Entry& entry = it->second;
        if (entry.texture != nullptr && entry.pixelArt != pixelArt) {
            applyFilter(*entry.texture, pixelArt);
            entry.pixelArt = pixelArt;
            ++m_generation;
        }
        return entry.texture;
    }

    // New, changed or vanished file: (re)load it.
    Entry entry;
    entry.stamp = stamp;
    entry.pixelArt = pixelArt;
    if (!ec) {
        auto* image = new ax::Image();
        if (image->initWithImageFile(key)) {
            auto* texture = new ax::Texture2D();
            if (texture->initWithImage(image)) {
                applyFilter(*texture, pixelArt);
                entry.texture = texture; // keeps the reference from `new`
            } else {
                texture->release();
            }
        }
        image->release();
    }
    if (entry.texture == nullptr) {
        m_log.warning(kCategory, "Could not load the image " + key + ".");
    }
    if (it != m_entries.end()) {
        AX_SAFE_RELEASE_NULL(it->second.texture);
        it->second = entry;
    } else {
        m_entries.emplace(key, entry);
    }
    ++m_generation;
    return entry.texture;
}

} // namespace rolo::detail
