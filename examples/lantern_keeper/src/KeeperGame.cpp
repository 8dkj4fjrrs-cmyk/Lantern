// The Lantern Keeper's game module: tells Rolo about the game's
// behaviours, so the editor can add them and the player can run them.
// See rolo/gameplay/GameModule.hpp.

#include "KeeperBehaviours.hpp"

#include "rolo/gameplay/GameModule.hpp"

ROLO_GAME_MODULE(keeper)
{
    registry.add(keeper::PlayerController::componentType());
    registry.add(keeper::SceneDoor::componentType());
    registry.add(keeper::SpawnPoint::componentType());
    registry.add(keeper::CameraLimits::componentType());
}
