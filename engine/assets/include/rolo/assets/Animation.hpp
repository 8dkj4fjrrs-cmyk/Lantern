#pragma once

// Flipbook animation: an image cut into equal frames (like a tileset), and
// named clips that show some of those frames one after another.
// Saved as its own asset file (.lanim):
//
//     { "format": "rolo.animations", "version": 1,
//       "image": "5a4d1e0000000007",            <- the sprite sheet, by ID
//       "frameSize": [16, 24], "margin": 0, "spacing": 0,
//       "clips": [
//         { "id": "walk_down", "frames": [1, 0, 2, 0], "fps": 8, "loop": true },
//         { "id": "idle_down", "frames": [0], "fps": 1, "loop": true } ] }
//
// Frames are numbered like tiles: from 0, left to right, then top to bottom.
// An Animator component (rolo::Animator) plays a clip on an object.

#include "rolo/assets/Tileset.hpp"   // TileRect
#include "rolo/scene/Components.hpp"   // Animator
#include "rolo/scene/EntityId.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rolo {

/// True for a usable clip id: letters, digits and _, starting with a letter
/// or _ (like ids in content files), at most 64 characters.
bool isValidClipId(std::string_view id);

struct AnimationClip {
    std::string id;            // e.g. "walk_left": letters, digits and _
    std::vector<int> frames;   // frame numbers in the sheet, in order
    float fps = 8.0f;          // frames shown per second
    bool loop = true;          // start again after the last frame (else stay on it)

    /// How long one pass through the clip takes, in seconds.
    double duration() const;
    /// The frame (number in the sheet) shown `seconds` after the clip
    /// started; -1 for a clip without frames.
    int frameAt(double seconds) const;
    /// True once a clip that does not loop has shown its last frame for its
    /// full time. Looping clips never finish.
    bool finishedAt(double seconds) const;

    friend bool operator==(const AnimationClip&, const AnimationClip&) = default;
};

struct AnimationSet {
    AssetId image;
    int frameWidth = 16;
    int frameHeight = 16;
    int margin = 0;    // pixels around the outside of the image
    int spacing = 0;   // pixels between neighbouring frames
    std::vector<AnimationClip> clips;

    const AnimationClip* findClip(std::string_view id) const;
    /// How many whole frames fit across and down an image of this size.
    int columns(int imageWidth) const;
    int rows(int imageHeight) const;
    /// Where frame `index` is in the image.
    TileRect frameRect(int index, int imageWidth) const;

    friend bool operator==(const AnimationSet&, const AnimationSet&) = default;
};

namespace AnimationFormat {
inline constexpr std::string_view kName = "rolo.animations";
inline constexpr int kVersion = 1;
inline constexpr std::string_view kExtension = ".lanim";
} // namespace AnimationFormat

/// The clip an Animator plays from `set`: the one it names, or the set's
/// first clip when it names none. Null if the set has no such clip.
const AnimationClip* animatorClip(const AnimationSet& set, const Animator& animator);

/// How far into its clip an Animator is, in seconds, when the animation
/// clock shows `clock`: scaled by its speed, and 0 while it is not playing.
double animatorTime(const Animator& animator, double clock);

/// The frame (number in the sheet) an Animator shows at `clock`; -1 if its
/// clip is missing or has no frames.
int animatorFrame(const AnimationSet& set, const Animator& animator, double clock);

/// Reads .lanim text. Nothing (and `error` set) if it cannot be used.
std::optional<AnimationSet> parseAnimationSet(std::string_view json, std::string* error = nullptr);
std::string serializeAnimationSet(const AnimationSet& set);

} // namespace rolo
