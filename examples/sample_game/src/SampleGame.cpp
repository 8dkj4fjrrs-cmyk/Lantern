// The sample game module: tells Rolo about the sample's behaviours.
// See rolo/gameplay/GameModule.hpp.

#include "SampleBehaviours.hpp"

#include "rolo/gameplay/GameModule.hpp"

ROLO_GAME_MODULE(sample_game)
{
    registry.add(sample::PlayerController::componentType());
    registry.add(sample::SceneDoor::componentType());
    registry.add(sample::Collectible::componentType());
    registry.add(sample::Spinner::componentType());
}
