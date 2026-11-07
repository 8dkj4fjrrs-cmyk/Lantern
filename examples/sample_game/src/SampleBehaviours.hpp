#pragma once

// The sample game's behaviours: small, commented examples of gameplay code.
//
//   PlayerController  walks with the arrow keys or W A S D, stopping at walls
//   SceneDoor         sends the player to another scene when they walk in
//   Collectible       disappears (and is counted) when the player touches it
//   Spinner           turns its object round and round

#include "rolo/gameplay/Behaviour.hpp"
#include "rolo/gameplay/Game.hpp"

#include <string>

namespace sample {

/// Moves its object with the arrow keys or W A S D. Give the object a
/// Collider (e.g. just around the feet) so walls and solid tiles stop it.
/// Stands still while a dialogue is shown.
///
/// With an Animator, it plays "walk_<facing>" while moving and
/// "idle_<facing>" while standing, facing down, left, right or up (the
/// sample hero's clips). Without one, it flips the Sprite to face left/right.
struct PlayerController : rolo::BehaviourBase<PlayerController> {
    float speed = 120.0f;   // points per second
    std::string facing = "down";   // not a property: where it looks while playing

    void update(rolo::Game& game, float dt) override;
    static const rolo::ComponentType& componentType();
};

/// Put on an object with a non-solid Collider, e.g. a doorway: when an
/// object with a PlayerController walks into it, the game changes to `scene`.
struct SceneDoor : rolo::BehaviourBase<SceneDoor> {
    rolo::AssetId scene;

    void update(rolo::Game& game, float dt) override;
    static const rolo::ComponentType& componentType();
};

/// Put on an object with a Collider: when the player touches it, it is
/// removed for good, the pick-up is written to the log (the editor's
/// Console), and 1 is added to the game variable named `variable` (if any).
/// The same can be done without code with a Trigger; this shows how C++
/// and content work together.
struct Collectible : rolo::BehaviourBase<Collectible> {
    std::string message = "Picked up a gem";
    std::string variable;

    void update(rolo::Game& game, float dt) override;
    static const rolo::ComponentType& componentType();
};

/// Turns its object, e.g. to make a gem catch the eye.
struct Spinner : rolo::BehaviourBase<Spinner> {
    float speed = 90.0f;   // degrees per second; negative turns the other way

    void update(rolo::Game& game, float dt) override;
    static const rolo::ComponentType& componentType();
};

} // namespace sample
