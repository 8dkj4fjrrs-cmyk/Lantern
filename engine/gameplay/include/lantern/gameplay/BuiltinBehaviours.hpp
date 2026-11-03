#pragma once

// Ready-made behaviours for top-down games, so a game can be made in the
// editor without writing C++ (the New Project wizard's Top-Down Starter
// uses them). They are ordinary behaviours: a game module can replace any
// of them with its own (see docs/gameplay.md).
//
//   TopDownController  walks with the arrow keys or W A S D, stopped by
//                      anything solid; plays walk_/idle_ clips if it can
//   SceneDoor          walking into it goes to another scene, arriving at
//                      an object named there (e.g. "From the Village")
//   CameraBounds       keeps the camera's view inside an area (the map)

#include "lantern/gameplay/Behaviour.hpp"
#include "lantern/scene/Component.hpp"

#include <string>

namespace lantern {

class ComponentRegistry;

/// Walks its object with the arrow keys or W A S D (give it a Collider so
/// walls stop it), and stands still during dialogues. With an Animator it
/// plays "walk_down", "walk_left", "walk_right", "walk_up" while moving and
/// "idle_..." while standing, when its animations have those clips; without
/// them it flips the Sprite to face left or right.
struct TopDownController : BehaviourBase<TopDownController> {
    float speed = 150.0f;          // points per second
    std::string facing = "down";   // not saved: where it looks while playing

    void update(Game& game, float dt) override;
    static const ComponentType& componentType();
};

/// A doorway: when the player (the object with the Interactor) walks into
/// this object's Collider (make it not solid), the game goes to `scene`,
/// and the player appears at the object named `arriveAt` there.
struct SceneDoor : BehaviourBase<SceneDoor> {
    AssetId scene;
    std::string arriveAt;

    void update(Game& game, float dt) override;
    static const ComponentType& componentType();
};

/// Put on the Camera (usually a child of the player): the view follows its
/// parent but stays inside the area from `min` to `max`, in world points,
/// so it never shows past the map's edges.
struct CameraBounds : BehaviourBase<CameraBounds> {
    Vec2 min{0.0f, 0.0f};
    Vec2 max{1920.0f, 1080.0f};
    Vec2 view{1280.0f, 720.0f};   // the game's view, in points (divided by the Camera's zoom)

    void update(Game& game, float dt) override;
    static const ComponentType& componentType();
};

/// Adds these behaviours to a registry (registerGameplayComponents does).
void registerBuiltinBehaviours(ComponentRegistry& registry);

} // namespace lantern
