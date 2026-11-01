#pragma once

// Game: a scene being played.
//
// The program running the game (the player, or the editor's play mode)
// creates a Game, starts it with a scene and calls update() once per frame.
// Behaviours receive the Game in start() and update() and use it to read
// input, move and find objects, ask about collisions and change scenes.
//
//     void PlayerController::update(lantern::Game& game, float dt)
//     {
//         const lantern::Vec2 direction = game.input().moveDirection();
//         game.move(entity(), direction * (speed * dt));   // stops at walls
//     }
//
// The game plays a *copy* of the scene it was started with. Whatever the game
// changes stays in that copy, so stopping it can never leave changes behind
// in the scene being edited.
//
// Errors: if a behaviour throws a C++ exception, the error is logged (the
// editor shows it in the Console) and that behaviour is switched off for the
// rest of the run; the rest of the game keeps going. A crash (such as using
// a null pointer) cannot be caught this way and ends the program.

#include "lantern/assets/AssetDatabase.hpp"
#include "lantern/core/Log.hpp"
#include "lantern/gameplay/Collision.hpp"
#include "lantern/gameplay/Input.hpp"
#include "lantern/scene/Scene.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lantern {

/// Loads a scene asset for a scene change. Returns nothing (and says why in
/// `error`) if it cannot.
using SceneLoader = std::function<std::optional<Scene>(AssetId scene, std::string& error)>;

class Game {
public:
    /// The longest frame time a game update uses, in seconds. After a pause
    /// (a slow disk, the debugger) time jumps; without a limit, things would
    /// move so far in one step that they pass through walls.
    static constexpr float kMaxStep = 0.1f;

    /// `assets`: the project's assets (tilesets for collision; may be null).
    /// `loader`: loads scenes for changeScene() (without one, scene changes
    /// are reported as errors).
    Game(const Logger& log, const AssetDatabase* assets, SceneLoader loader = {});
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // --- Running the game (the player or the editor) --------------------------------------------
    /// Starts playing a copy of `scene`.
    void start(const Scene& scene);
    /// Runs one frame: behaviours' start() and update(), then removals and
    /// a requested scene change, then clears this frame's input flags.
    void update(float dt);
    /// Ends the game; its scene copy is dropped.
    void stop();
    bool isRunning() const noexcept { return m_scene != nullptr; }

    /// The scene being played, for drawing. A different object after a
    /// scene change. Null when not running.
    std::shared_ptr<const Scene> sharedScene() const { return m_scene; }
    /// Frames run since start().
    std::uint64_t frameCount() const noexcept { return m_frames; }
    /// Where the program reports keys and the mouse.
    Input& input() noexcept { return m_input; }

    // --- For behaviours -----------------------------------------------------------------------
    /// The scene being played (a copy; see above). Must be running.
    Scene& scene() { return *m_scene; }
    const Scene& scene() const { return *m_scene; }
    const Input& input() const noexcept { return m_input; }
    /// Seconds since start() (game time: does not advance while paused).
    double time() const noexcept { return m_time; }
    /// The length of the current frame, in seconds.
    float deltaTime() const noexcept { return m_dt; }
    const Logger& log() const noexcept { return m_log; }
    const AssetDatabase* assets() const noexcept { return m_assets; }

    /// The first object with this name (in hierarchy order); null ID if none.
    EntityId find(std::string_view name) const;
    /// Every object that has a component of type T, in hierarchy order.
    template <class T>
    std::vector<EntityId> findAll() const
    {
        return findAllWith(T::componentType().id);
    }
    /// A component of an object, to change (null if missing). Changing a
    /// Transform this way only moves things; anything else redraws the object.
    template <class T>
    T* get(EntityId id)
    {
        return m_scene != nullptr ? m_scene->edit<T>(id) : nullptr;
    }
    /// An object's Transform, to move, turn or scale it.
    Transform* transform(EntityId id) { return get<Transform>(id); }
    /// An object's position in the world (parents included).
    Vec2 worldPosition(EntityId id) const;
    /// Removes an object (and its children) at the end of this frame.
    void destroy(EntityId id);
    /// Switches to another scene at the end of this frame. The current
    /// scene's objects (and their behaviours) are dropped; the new scene's
    /// behaviours start in the next frame.
    void changeScene(AssetId scene);

    // --- Collision queries (see Collision.hpp) ----------------------------------------------------
    /// The world box of an object's Collider, if it has one.
    std::optional<Box> colliderBox(EntityId id) const;
    /// Objects whose colliders overlap this object's collider.
    std::vector<EntityId> overlapping(EntityId id) const;
    /// Objects whose colliders overlap a box.
    std::vector<EntityId> overlapping(const Box& box, EntityId ignore = {}) const;
    /// True if something solid (a solid collider or tile) overlaps the box.
    bool isBlocked(const Box& box, EntityId ignore = {}) const;
    /// Moves an object by `delta` (world points), stopping at anything solid
    /// in the way of its collider and sliding along walls. Without a
    /// collider it simply moves.
    MoveResult move(EntityId id, Vec2 delta);

private:
    std::vector<EntityId> findAllWith(std::string_view typeId) const;
    void runBehaviours();
    void finishFrame();
    bool isActiveInScene(const Entity& entity) const;

    const Logger& m_log;
    const AssetDatabase* m_assets;
    SceneLoader m_loader;
    std::shared_ptr<Scene> m_scene;
    Input m_input;
    double m_time = 0.0;
    float m_dt = 0.0f;
    std::uint64_t m_frames = 0;
    std::vector<EntityId> m_toDestroy;
    std::optional<AssetId> m_nextScene;
};

} // namespace lantern
