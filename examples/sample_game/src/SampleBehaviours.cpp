#include "SampleBehaviours.hpp"

#include <algorithm>

namespace sample {

using namespace lantern;

namespace {

// True if any of `ids` has a PlayerController.
bool containsPlayer(const Game& game, const std::vector<EntityId>& ids)
{
    return std::any_of(ids.begin(), ids.end(), [&](EntityId id) {
        const Entity* e = game.scene().find(id);
        return e != nullptr && e->get<PlayerController>() != nullptr;
    });
}

} // namespace

// --- PlayerController ------------------------------------------------------------------------

void PlayerController::update(Game& game, float dt)
{
    if (game.isInDialogue()) {
        return; // the arrow keys choose answers now
    }
    const Vec2 direction = game.input().moveDirection();
    if (direction.x == 0.0f && direction.y == 0.0f) {
        return;
    }
    // move() slides along walls and solid tiles instead of going through.
    game.move(entity(), direction * (speed * dt));

    // Face the way we walk. Only change the sprite when the facing changes:
    // changing a sprite redraws it, moving does not.
    const Entity* self = game.scene().find(entity());
    const Sprite* sprite = self != nullptr ? self->get<Sprite>() : nullptr;
    const bool faceLeft = direction.x < 0.0f;
    if (sprite != nullptr && direction.x != 0.0f && sprite->flipX != faceLeft) {
        game.get<Sprite>(entity())->flipX = faceLeft;
    }
}

const ComponentType& PlayerController::componentType()
{
    static const ComponentType type =
        ComponentTypeBuilder<PlayerController>("sample.PlayerController", "Player Controller")
            .category("Gameplay")
            .description("Walks with the arrow keys or W A S D. Give the object a Collider so walls stop it.")
            .property("speed", "Speed", &PlayerController::speed,
                      {.min = 0.0f, .step = 10.0f, .help = "How fast it walks, in points per second.", .unit = "pt/s"})
            .build();
    return type;
}

// --- SceneDoor -------------------------------------------------------------------------------

void SceneDoor::update(Game& game, float)
{
    if (scene.isNull()) {
        return;
    }
    if (containsPlayer(game, game.overlapping(entity()))) {
        game.changeScene(scene);
    }
}

const ComponentType& SceneDoor::componentType()
{
    static const ComponentType type =
        ComponentTypeBuilder<SceneDoor>("sample.SceneDoor", "Scene Door")
            .category("Gameplay")
            .description("When the player walks into this object's Collider (make it not solid), the game "
                         "changes to another scene.")
            .property("scene", "Scene", &SceneDoor::scene,
                      {.help = "The scene to go to.", .assetType = "scene"})
            .build();
    return type;
}

// --- Collectible -----------------------------------------------------------------------------

void Collectible::update(Game& game, float)
{
    if (containsPlayer(game, game.overlapping(entity()))) {
        game.log().info("game", message);
        if (!variable.empty()) {
            Action add;
            add.kind = Action::Kind::Add;
            add.variable = variable;
            add.value = 1;
            game.content().run({add});   // the same action content uses
        }
        game.destroy(entity());   // stays collected for the rest of the playthrough
    }
}

const ComponentType& Collectible::componentType()
{
    static const ComponentType type =
        ComponentTypeBuilder<Collectible>("sample.Collectible", "Collectible")
            .category("Gameplay")
            .description("Disappears when the player touches its Collider, and writes a message to the Console.")
            .property("message", "Message", &Collectible::message, {.help = "Written to the Console when picked up."})
            .property("variable", "Adds To", &Collectible::variable,
                      {.help = "A whole-number game variable to add 1 to, e.g. gems. Empty: none."})
            .build();
    return type;
}

// --- Spinner ---------------------------------------------------------------------------------

void Spinner::update(Game& game, float dt)
{
    game.transform(entity())->rotation += speed * dt;
}

const ComponentType& Spinner::componentType()
{
    static const ComponentType type =
        ComponentTypeBuilder<Spinner>("sample.Spinner", "Spinner")
            .category("Gameplay")
            .description("Turns the object round and round.")
            .property("speed", "Speed", &Spinner::speed, {.step = 15.0f, .help = "Degrees per second.", .unit = "°/s"})
            .build();
    return type;
}

} // namespace sample
