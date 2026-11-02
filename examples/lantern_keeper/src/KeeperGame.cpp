// The Lantern Keeper's game module: tells Lantern about the game's
// behaviours, so the editor can add them and the player can run them.
// See lantern/gameplay/GameModule.hpp.

#include "KeeperBehaviours.hpp"

#include "lantern/gameplay/GameModule.hpp"

LANTERN_GAME_MODULE(keeper)
{
    registry.add(keeper::PlayerController::componentType());
    registry.add(keeper::SceneDoor::componentType());
    registry.add(keeper::SpawnPoint::componentType());
    registry.add(keeper::CameraLimits::componentType());
}
