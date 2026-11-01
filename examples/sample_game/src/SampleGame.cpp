// The sample game module: tells Lantern about the sample's behaviours.
// See lantern/gameplay/GameModule.hpp.

#include "SampleBehaviours.hpp"

#include "lantern/gameplay/GameModule.hpp"

LANTERN_GAME_MODULE(sample_game)
{
    registry.add(sample::PlayerController::componentType());
    registry.add(sample::SceneDoor::componentType());
    registry.add(sample::Collectible::componentType());
    registry.add(sample::Spinner::componentType());
}
