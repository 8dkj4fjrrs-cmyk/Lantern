#pragma once

// The sample game's behaviours: small, commented examples of gameplay code.
//
//   PlayerController  walks with the arrow keys or W A S D, stopping at walls
//   SceneDoor         sends the player to another scene when they walk in
//   Collectible       disappears (and is counted) when the player touches it
//   Spinner           turns its object round and round

#include "lantern/gameplay/Behaviour.hpp"
#include "lantern/gameplay/Game.hpp"

namespace sample {

/// Moves its object with the arrow keys or W A S D. Give the object a
/// Collider (e.g. just around the feet) so walls and solid tiles stop it.
/// Stands still while a dialogue is shown.
struct PlayerController : lantern::BehaviourBase<PlayerController> {
    float speed = 120.0f;   // points per second

    void update(lantern::Game& game, float dt) override;
    static const lantern::ComponentType& componentType();
};

/// Put on an object with a non-solid Collider, e.g. a doorway: when an
/// object with a PlayerController walks into it, the game changes to `scene`.
struct SceneDoor : lantern::BehaviourBase<SceneDoor> {
    lantern::AssetId scene;

    void update(lantern::Game& game, float dt) override;
    static const lantern::ComponentType& componentType();
};

/// Put on an object with a Collider: when the player touches it, it is
/// removed for good, the pick-up is written to the log (the editor's
/// Console), and 1 is added to the game variable named `variable` (if any).
/// The same can be done without code with a Trigger; this shows how C++
/// and content work together.
struct Collectible : lantern::BehaviourBase<Collectible> {
    std::string message = "Picked up a gem";
    std::string variable;

    void update(lantern::Game& game, float dt) override;
    static const lantern::ComponentType& componentType();
};

/// Turns its object, e.g. to make a gem catch the eye.
struct Spinner : lantern::BehaviourBase<Spinner> {
    float speed = 90.0f;   // degrees per second; negative turns the other way

    void update(lantern::Game& game, float dt) override;
    static const lantern::ComponentType& componentType();
};

} // namespace sample
