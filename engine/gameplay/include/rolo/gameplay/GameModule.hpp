#pragma once

// Game modules: how a game's C++ code joins Rolo.
//
// A game module is a static library with the game's behaviours. It has one
// registration function that tells Rolo about them:
//
//     // mygame/MyGame.cpp
//     #include "rolo/gameplay/GameModule.hpp"
//
//     ROLO_GAME_MODULE(my_game)
//     {
//         registry.add(Spinner::componentType());
//         registry.add(PlayerController::componentType());
//     }
//
// and is declared in CMake with:
//
//     rolo_add_game_module(my_game SOURCES MyGame.cpp Spinner.cpp ...)
//
// The editor and the player are then built with the module linked in (see
// docs/gameplay.md). At start-up they call registerGameModules(), which
// calls every linked module's function, so the editor's Inspector offers the
// behaviours and the player can run them. Modules are linked when Rolo is
// built: there is no loading of code while the editor runs (no hot reload)
// yet, which keeps this simple and the same on every platform.

#include "rolo/core/Log.hpp"
#include "rolo/scene/Component.hpp"

#include <span>
#include <string>
#include <string_view>

namespace rolo {

/// What a game module's registration function receives.
class GameModuleRegistry {
public:
    GameModuleRegistry(ComponentRegistry& registry, const Logger& log, std::string_view module);

    /// Adds a component or behaviour type. A problem (such as a type ID that
    /// is already taken) is reported in the log and the type is skipped.
    void add(const ComponentType& type);
    /// The name of the module being registered.
    const std::string& module() const noexcept { return m_module; }

private:
    ComponentRegistry& m_registry;
    const Logger& m_log;
    std::string m_module;
};

} // namespace rolo

/// Defines a game module's registration function. The body receives
/// `registry` (a rolo::GameModuleRegistry&) to add the module's types to.
/// `name` must be the CMake target name given to rolo_add_game_module.
#define ROLO_GAME_MODULE(name) void roloRegisterGameModule_##name(::rolo::GameModuleRegistry& registry)

namespace rolo {

/// The names of the game modules linked into this program.
std::span<const std::string_view> linkedGameModules();

/// Lets every linked game module add its types to `registry`, and reports
/// problems (e.g. two modules using the same type ID) to `log`.
void registerGameModules(ComponentRegistry& registry, const Logger& log);

} // namespace rolo
