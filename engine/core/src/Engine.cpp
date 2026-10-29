#include "lantern/core/Engine.hpp"

#include "lantern/core/Version.hpp"

#include <string>
#include <utility>

namespace lantern {

namespace {
constexpr std::string_view kCategory = "engine";
}

Engine::Engine(EngineConfig config)
    : m_config(std::move(config))
{
}

Engine::~Engine()
{
    // RAII: an engine that goes out of scope always shuts down cleanly.
    shutdown();
}

Status Engine::initialize()
{
    switch (m_state) {
    case State::Running:
        return Status::failure("Engine::initialize() called while the engine is already running.");
    case State::ShutDown:
        return Status::failure("Engine::initialize() called after shutdown; create a new Engine instead.");
    case State::Created:
        break;
    }

    m_logger.info(kCategory, "Starting '" + m_config.applicationName + "' with Lantern "
                                 + std::string(versionString()) + " (backend: "
                                 + std::string(runtimeBackendName()) + ")");

    // Milestone 1: initialize the runtime backend (window, renderer, audio) here.

    m_state = State::Running;
    return Status::success();
}

void Engine::shutdown()
{
    if (m_state != State::Running) {
        return;
    }

    // Milestone 1: shut subsystems down in reverse order of initialization.

    m_state = State::ShutDown;
    m_logger.info(kCategory, "Shut down '" + m_config.applicationName + "'");
}

std::string_view toString(Engine::State state) noexcept
{
    switch (state) {
    case Engine::State::Created: return "Created";
    case Engine::State::Running: return "Running";
    case Engine::State::ShutDown: return "ShutDown";
    }
    return "Unknown";
}

} // namespace lantern
