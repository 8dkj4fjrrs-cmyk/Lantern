#pragma once

// The Lantern Keeper's behaviours: the little C++ the game needs. Everything
// else (talking, quests, picking up flames, the gate, saving, the ending) is
// content: triggers, dialogues and quests made in the editor.
//
//   PlayerController  walks with the arrow keys or W A S D and plays the
//                     hero's walk_<dir> / idle_<dir> clips
//   SceneDoor         walking into it goes to another scene, arriving at a
//                     named SpawnPoint there
//   SpawnPoint        where the player appears when arriving through a door
//   CameraLimits      keeps the camera inside the map, so the view never
//                     shows the empty space beyond its edges

#include "rolo/gameplay/Behaviour.hpp"
#include "rolo/gameplay/Game.hpp"

#include <string>

namespace keeper {

/// The content variable (a "text" variable in the game's .lvars) that tells
/// the next scene where the player arrives: a SpawnPoint's name.
inline constexpr const char* kArrivalVariable = "arrival";

/// Moves its object with the arrow keys or W A S D, stopping at walls (give
/// it a Collider). Stands still during dialogues. With an Animator it plays
/// "walk_<facing>" while moving and "idle_<facing>" while standing.
struct PlayerController : rolo::BehaviourBase<PlayerController> {
    float speed = 150.0f;          // points per second
    std::string facing = "down";   // not saved: where it looks while playing

    void update(rolo::Game& game, float dt) override;
    static const rolo::ComponentType& componentType();
};

/// A doorway: when the player walks into this object's Collider (make it not
/// solid), the game changes to `scene` and the player appears at the
/// SpawnPoint there named `arriveAt`.
struct SceneDoor : rolo::BehaviourBase<SceneDoor> {
    rolo::AssetId scene;
    std::string arriveAt;

    void update(rolo::Game& game, float dt) override;
    static const rolo::ComponentType& componentType();
};

/// Where the player appears when arriving through a SceneDoor whose
/// "Arrive At" is this object's name. (Starting the game or loading a save
/// leaves the player where the scene or the save puts them.)
struct SpawnPoint : rolo::BehaviourBase<SpawnPoint> {
    void start(rolo::Game& game) override;
    static const rolo::ComponentType& componentType();
};

/// Put on the Camera (a child of the player): keeps the view inside the
/// rectangle from `min` to `max` (world points), e.g. the map's edges.
struct CameraLimits : rolo::BehaviourBase<CameraLimits> {
    rolo::Vec2 min{0.0f, 0.0f};
    rolo::Vec2 max{1920.0f, 1080.0f};
    rolo::Vec2 view{1280.0f, 720.0f};   // the game's view size, in points

    void update(rolo::Game& game, float dt) override;
    static const rolo::ComponentType& componentType();
};

} // namespace keeper
