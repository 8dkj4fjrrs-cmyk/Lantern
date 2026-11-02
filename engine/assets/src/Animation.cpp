#include "lantern/assets/Animation.hpp"

#include "lantern/core/Numbers.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <set>

namespace lantern {

namespace {

using Json = nlohmann::ordered_json;

void setError(std::string* error, std::string message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

// Speeds are written as typed: 8 (not 8.0), 7.5 (not 7.5000000001).
Json fpsJson(float fps)
{
    const double shortest = parseNumber(formatShortest(fps)).value_or(8.0);
    if (shortest == std::trunc(shortest)) {
        return static_cast<int>(shortest);
    }
    return shortest;
}

} // namespace

// Same rule as content ids (lantern_content's isValidContentId).
bool isValidClipId(std::string_view id)
{
    if (id.empty() || id.size() > 64) {
        return false;
    }
    const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    return letter(id.front())
           && std::all_of(id.begin(), id.end(), [&](char c) { return letter(c) || (c >= '0' && c <= '9'); });
}


// --- Clips -----------------------------------------------------------------------------------------

double AnimationClip::duration() const
{
    return fps > 0.0f ? static_cast<double>(frames.size()) / static_cast<double>(fps) : 0.0;
}

int AnimationClip::frameAt(double seconds) const
{
    if (frames.empty()) {
        return -1;
    }
    if (fps <= 0.0f || seconds <= 0.0) {
        return frames.front();
    }
    const auto count = static_cast<long long>(frames.size());
    long long step = static_cast<long long>(std::floor(seconds * static_cast<double>(fps)));
    step = loop ? step % count : std::min(step, count - 1);
    return frames[static_cast<std::size_t>(step)];
}

bool AnimationClip::finishedAt(double seconds) const
{
    return !loop && seconds >= duration();
}

// --- Sets ----------------------------------------------------------------------------------------

const AnimationClip* AnimationSet::findClip(std::string_view id) const
{
    const auto it = std::find_if(clips.begin(), clips.end(), [&](const AnimationClip& c) { return c.id == id; });
    return it != clips.end() ? &*it : nullptr;
}

int AnimationSet::columns(int imageWidth) const
{
    Tileset grid;
    grid.tileWidth = frameWidth;
    grid.tileHeight = frameHeight;
    grid.margin = margin;
    grid.spacing = spacing;
    return grid.columns(imageWidth);   // the same grid as tilesets
}

int AnimationSet::rows(int imageHeight) const
{
    Tileset grid;
    grid.tileWidth = frameWidth;
    grid.tileHeight = frameHeight;
    grid.margin = margin;
    grid.spacing = spacing;
    return grid.rows(imageHeight);
}

TileRect AnimationSet::frameRect(int index, int imageWidth) const
{
    Tileset grid;
    grid.tileWidth = frameWidth;
    grid.tileHeight = frameHeight;
    grid.margin = margin;
    grid.spacing = spacing;
    return grid.tileRect(index, imageWidth);
}

// --- Files -----------------------------------------------------------------------------------------

std::optional<AnimationSet> parseAnimationSet(std::string_view text, std::string* error)
{
    Json j;
    try {
        j = Json::parse(text.begin(), text.end());
    } catch (const nlohmann::json::parse_error&) {
        setError(error, "The animation file is not valid JSON.");
        return std::nullopt;
    }
    if (!j.is_object() || j.value("format", "") != AnimationFormat::kName) {
        setError(error, "This is not a Lantern animation file (\"format\" should be \"lantern.animations\").");
        return std::nullopt;
    }
    if (!j.contains("version") || !j["version"].is_number_integer() || j["version"].get<int>() < 1) {
        setError(error, "The animation file has no valid format \"version\".");
        return std::nullopt;
    }
    if (j["version"].get<int>() > AnimationFormat::kVersion) {
        setError(error, "This animation file was saved by a newer version of Lantern. Update Lantern to use it.");
        return std::nullopt;
    }
    AnimationSet set;
    if (j.contains("image") && !j["image"].is_null()) {
        const auto id = j["image"].is_string() ? AssetId::parse(j["image"].get<std::string>()) : std::nullopt;
        if (!id) {
            setError(error, "\"image\" should be the ID of an image asset (16 hex digits).");
            return std::nullopt;
        }
        set.image = *id;
    }
    if (!j.contains("frameSize") || !j["frameSize"].is_array() || j["frameSize"].size() != 2
        || !j["frameSize"][0].is_number_integer() || !j["frameSize"][1].is_number_integer()) {
        setError(error, "\"frameSize\" should be two whole numbers, like [16, 24].");
        return std::nullopt;
    }
    set.frameWidth = j["frameSize"][0].get<int>();
    set.frameHeight = j["frameSize"][1].get<int>();
    if (set.frameWidth < 1 || set.frameHeight < 1 || set.frameWidth > 4096 || set.frameHeight > 4096) {
        setError(error, "Frames must be between 1 and 4096 pixels wide and high.");
        return std::nullopt;
    }
    for (const char* key : {"margin", "spacing"}) {
        if (j.contains(key)) {
            if (!j[key].is_number_integer() || j[key].get<long long>() < 0 || j[key].get<long long>() > 4096) {
                setError(error, "\"margin\" and \"spacing\" should be whole numbers of pixels (0 or more).");
                return std::nullopt;
            }
            (std::string_view(key) == "margin" ? set.margin : set.spacing) = j[key].get<int>();
        }
    }
    if (j.contains("clips")) {
        if (!j["clips"].is_array()) {
            setError(error, "\"clips\" should be a list.");
            return std::nullopt;
        }
        std::set<std::string> ids;
        for (const Json& c : j["clips"]) {
            AnimationClip clip;
            if (!c.is_object() || !c.contains("id") || !c["id"].is_string() || !isValidClipId(c["id"].get<std::string>())) {
                setError(error, "Each clip needs an \"id\": letters, digits and _, starting with a letter or _.");
                return std::nullopt;
            }
            clip.id = c["id"].get<std::string>();
            if (!ids.insert(clip.id).second) {
                setError(error, "Two clips are called \"" + clip.id + "\"; clip ids must be unique.");
                return std::nullopt;
            }
            if (c.contains("frames")) {
                if (!c["frames"].is_array()) {
                    setError(error, "Clip \"" + clip.id + "\": \"frames\" should be a list of frame numbers.");
                    return std::nullopt;
                }
                for (const Json& f : c["frames"]) {
                    if (!f.is_number_integer() || f.get<long long>() < 0 || f.get<long long>() > 1'000'000) {
                        setError(error, "Clip \"" + clip.id + "\": frames are numbers from 0.");
                        return std::nullopt;
                    }
                    clip.frames.push_back(f.get<int>());
                }
            }
            if (c.contains("fps")) {
                if (!c["fps"].is_number() || c["fps"].get<double>() <= 0.0 || c["fps"].get<double>() > 240.0) {
                    setError(error, "Clip \"" + clip.id + "\": \"fps\" should be a number from above 0 to 240.");
                    return std::nullopt;
                }
                clip.fps = c["fps"].get<float>();
            }
            if (c.contains("loop")) {
                if (!c["loop"].is_boolean()) {
                    setError(error, "Clip \"" + clip.id + "\": \"loop\" should be true or false.");
                    return std::nullopt;
                }
                clip.loop = c["loop"].get<bool>();
            }
            set.clips.push_back(std::move(clip));
        }
    }
    return set;
}

std::string serializeAnimationSet(const AnimationSet& set)
{
    Json j;
    j["format"] = AnimationFormat::kName;
    j["version"] = AnimationFormat::kVersion;
    j["image"] = set.image.isNull() ? Json() : Json(set.image.toString());
    j["frameSize"] = Json::array({set.frameWidth, set.frameHeight});
    j["margin"] = set.margin;
    j["spacing"] = set.spacing;
    Json clips = Json::array();
    for (const AnimationClip& c : set.clips) {
        clips.push_back(Json{{"id", c.id}, {"frames", c.frames}, {"fps", fpsJson(c.fps)}, {"loop", c.loop}});
    }
    j["clips"] = std::move(clips);
    return j.dump(2) + "\n";
}

const AnimationClip* animatorClip(const AnimationSet& set, const Animator& animator)
{
    if (animator.clip.empty()) {
        return set.clips.empty() ? nullptr : &set.clips.front();
    }
    return set.findClip(animator.clip);
}

double animatorTime(const Animator& animator, double clock)
{
    if (!animator.playing) {
        return 0.0;
    }
    return std::max(0.0, (clock - animator.startTime) * static_cast<double>(animator.speed));
}

int animatorFrame(const AnimationSet& set, const Animator& animator, double clock)
{
    const AnimationClip* clip = animatorClip(set, animator);
    return clip != nullptr ? clip->frameAt(animatorTime(animator, clock)) : -1;
}

} // namespace lantern
