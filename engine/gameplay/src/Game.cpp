#include "lantern/gameplay/Game.hpp"

#include "lantern/gameplay/Behaviour.hpp"

#include <algorithm>
#include <exception>

namespace lantern {

namespace {
constexpr std::string_view kCategory = "game";
}

Game::Game(const Logger& log, const AssetDatabase* assets, SceneLoader loader)
    : m_log(log)
    , m_assets(assets)
    , m_loader(std::move(loader))
{
}

Game::~Game() = default;

void Game::start(const Scene& scene)
{
    m_scene = std::make_shared<Scene>(scene);
    m_time = 0.0;
    m_dt = 0.0f;
    m_frames = 0;
    m_toDestroy.clear();
    m_nextScene.reset();
    m_input.releaseAll();
    m_input.endFrame();
}

void Game::stop()
{
    m_scene.reset();
    m_toDestroy.clear();
    m_nextScene.reset();
    m_input.releaseAll();
    m_input.endFrame();
}

void Game::update(float dt)
{
    if (m_scene == nullptr) {
        return;
    }
    m_dt = std::clamp(dt, 0.0f, kMaxStep);
    m_time += static_cast<double>(m_dt);
    runBehaviours();
    finishFrame();
    ++m_frames;
    m_input.endFrame();
}

bool Game::isActiveInScene(const Entity& entity) const
{
    for (const Entity* e = &entity; e != nullptr; e = m_scene->find(e->parent())) {
        if (!e->isActive()) {
            return false;
        }
    }
    return true;
}

void Game::runBehaviours()
{
    // Who to run is decided first: behaviours may add or remove objects
    // while they run, which must not disturb the walk through the scene.
    struct Call {
        EntityId entity;
        std::string type;
    };
    std::vector<Call> calls;
    const Scene& scene = *m_scene;
    scene.forEach([&](const Entity& entity, int) {
        for (const auto& component : entity.components()) {
            if (dynamic_cast<const Behaviour*>(component.get()) != nullptr) {
                calls.push_back({entity.id(), component->type().id});
            }
        }
    });

    const std::shared_ptr<Scene> playing = m_scene; // a behaviour may stop the game
    for (const Call& call : calls) {
        if (m_scene != playing) {
            return;
        }
        const Entity* entity = m_scene->find(call.entity);
        if (entity == nullptr || !isActiveInScene(*entity)) {
            continue; // removed or switched off earlier this frame
        }
        // A behaviour's own fields are not drawn, so running it is not a
        // change to the scene (changes it makes through Game or Scene are).
        auto* behaviour = const_cast<Behaviour*>(dynamic_cast<const Behaviour*>(entity->component(call.type)));
        if (behaviour == nullptr || behaviour->m_failed) {
            continue;
        }
        behaviour->m_entity = call.entity;
        const bool starting = !behaviour->m_started;
        try {
            if (starting) {
                behaviour->m_started = true;
                behaviour->start(*this);
            }
            behaviour->update(*this, m_dt);
        } catch (const std::exception& error) {
            behaviour->m_failed = true;
            m_log.error(kCategory, behaviour->type().displayName + " on '" + entity->name() + "' failed in "
                                       + (starting ? "start()" : "update()") + ": " + error.what()
                                       + ". It is switched off until the game is played again.");
        } catch (...) {
            behaviour->m_failed = true;
            m_log.error(kCategory, behaviour->type().displayName + " on '" + entity->name()
                                       + "' failed with an unknown error. It is switched off until the game is "
                                         "played again.");
        }
    }
}

void Game::finishFrame()
{
    for (const EntityId id : m_toDestroy) {
        m_scene->remove(id);   // false if already gone (e.g. inside something removed before)
    }
    m_toDestroy.clear();

    if (!m_nextScene) {
        return;
    }
    const AssetId next = *m_nextScene;
    m_nextScene.reset();
    std::string error = "this program cannot load scenes";
    std::optional<Scene> loaded = m_loader ? m_loader(next, error) : std::nullopt;
    if (!loaded) {
        m_log.error(kCategory, "Could not change to scene " + next.toString() + ": " + error);
        return;
    }
    m_log.info(kCategory, "Changed to scene '" + loaded->name() + "'.");
    m_scene = std::make_shared<Scene>(std::move(*loaded));
}

EntityId Game::find(std::string_view name) const
{
    EntityId found;
    if (m_scene != nullptr) {
        m_scene->forEach([&](const Entity& entity, int) {
            if (found.isNull() && entity.name() == name) {
                found = entity.id();
            }
        });
    }
    return found;
}

std::vector<EntityId> Game::findAllWith(std::string_view typeId) const
{
    std::vector<EntityId> out;
    if (m_scene != nullptr) {
        m_scene->forEach([&](const Entity& entity, int) {
            if (entity.component(typeId) != nullptr) {
                out.push_back(entity.id());
            }
        });
    }
    return out;
}

Vec2 Game::worldPosition(EntityId id) const
{
    return m_scene != nullptr ? m_scene->worldTransform(id).apply({0.0f, 0.0f}) : Vec2{};
}

void Game::destroy(EntityId id)
{
    if (std::find(m_toDestroy.begin(), m_toDestroy.end(), id) == m_toDestroy.end()) {
        m_toDestroy.push_back(id);
    }
}

void Game::changeScene(AssetId scene)
{
    m_nextScene = scene;
}

std::optional<Box> Game::colliderBox(EntityId id) const
{
    return m_scene != nullptr ? lantern::colliderBox(*m_scene, id) : std::nullopt;
}

std::vector<EntityId> Game::overlapping(EntityId id) const
{
    const auto box = colliderBox(id);
    return box ? overlappingColliders(*m_scene, *box, id) : std::vector<EntityId>{};
}

std::vector<EntityId> Game::overlapping(const Box& box, EntityId ignore) const
{
    return m_scene != nullptr ? overlappingColliders(*m_scene, box, ignore) : std::vector<EntityId>{};
}

bool Game::isBlocked(const Box& box, EntityId ignore) const
{
    return m_scene != nullptr && lantern::isBlocked(*m_scene, m_assets, box, ignore);
}

MoveResult Game::move(EntityId id, Vec2 delta)
{
    MoveResult result;
    const Entity* entity = m_scene != nullptr ? m_scene->find(id) : nullptr;
    if (entity == nullptr) {
        return result;
    }
    if (const auto box = colliderBox(id)) {
        result = sweepBox(*m_scene, m_assets, *box, delta, id);
    } else {
        result.moved = delta;
    }
    if (result.moved.x == 0.0f && result.moved.y == 0.0f) {
        return result;
    }
    // `moved` is in world points; the position is relative to the parent.
    Vec2 local = result.moved;
    if (const Entity* parent = m_scene->find(entity->parent())) {
        if (const auto inverse = m_scene->worldTransform(parent->id()).inverse()) {
            local = inverse->apply(result.moved) - inverse->apply({0.0f, 0.0f});
        }
    }
    if (Transform* t = m_scene->edit<Transform>(id)) {
        t->position = t->position + local;
    }
    return result;
}

} // namespace lantern
