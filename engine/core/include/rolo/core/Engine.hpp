#pragma once

#include "rolo/core/Log.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace rolo {

/// Settings chosen by whoever creates the engine (the editor, the game
/// player, or a test). Kept as plain data so it can later be loaded from a
/// project file.
struct EngineConfig {
    std::string applicationName = "Rolo Application";
};

/// Outcome of an operation that can fail. `message` explains failures in
/// words a user can act on.
struct Status {
    bool ok = true;
    std::string message;

    static Status success() { return {}; }
    static Status failure(std::string why) { return {false, std::move(why)}; }
    explicit operator bool() const noexcept { return ok; }
};

/// The engine's top-level lifetime object.
///
/// Typical use:
///     rolo::Engine engine({.applicationName = "My Game"});
///     engine.logger().addSink(rolo::makeConsoleSink());
///     if (auto status = engine.initialize(); !status) { /* report */ }
///     ...
///     engine.shutdown();   // also happens automatically in the destructor
///
/// In Milestone 0 this only manages lifecycle and logging. Later milestones
/// will start the runtime backend, window, and subsystems from initialize().
class Engine {
public:
    enum class State {
        Created,     // constructed, not yet initialized
        Running,     // initialize() succeeded
        ShutDown,    // shutdown() completed
    };

    explicit Engine(EngineConfig config = {});
    ~Engine();

    // One engine object owns real resources, so copying makes no sense.
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    /// Starts the engine. Fails (without side effects) if called twice or
    /// after shutdown.
    Status initialize();

    /// Stops the engine. Safe to call more than once.
    void shutdown();

    State state() const noexcept { return m_state; }
    bool isRunning() const noexcept { return m_state == State::Running; }

    const EngineConfig& config() const noexcept { return m_config; }
    Logger& logger() noexcept { return m_logger; }
    const Logger& logger() const noexcept { return m_logger; }

private:
    EngineConfig m_config;
    Logger m_logger;
    State m_state = State::Created;
};

std::string_view toString(Engine::State state) noexcept;

} // namespace rolo
