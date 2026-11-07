#include "rolo/gameplay/GameModule.hpp"

namespace rolo {

GameModuleRegistry::GameModuleRegistry(ComponentRegistry& registry, const Logger& log, std::string_view module)
    : m_registry(registry)
    , m_log(log)
    , m_module(module)
{
}

void GameModuleRegistry::add(const ComponentType& type)
{
    if (const Status status = m_registry.add(type); !status) {
        m_log.error("game", "Game module '" + m_module + "' could not add '" + type.id + "': " + status.message);
    }
}

} // namespace rolo
