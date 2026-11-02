#include "lantern/gameplay/Game.hpp"

#include "lantern/gameplay/Behaviour.hpp"
#include "lantern/gameplay/Components.hpp"

#include <algorithm>
#include <exception>

namespace lantern {

namespace {
constexpr std::string_view kCategory = "game";
}

Game::Game(const Logger& log, GameSetup setup)
    : m_log(log)
    , m_setup(std::move(setup))
{
    if (m_setup.content == nullptr) {
        m_ownContent = std::make_unique<ContentLibrary>();
        m_setup.content = m_ownContent.get();
    }
    m_runner = std::make_unique<ContentRunner>(*m_setup.content, m_log);
    m_runner->setHost(this);
}

Game::Game(const Logger& log, const AssetDatabase* assets, SceneLoader loader)
    : Game(log, GameSetup{assets, std::move(loader), nullptr, nullptr, "Game"})
{
}

Game::~Game() = default;

void Game::start(const Scene& scene, AssetId sceneId)
{
    m_time = 0.0;
    m_dt = 0.0f;
    m_frames = 0;
    m_toDestroy.clear();
    m_nextScene.reset();
    m_loadSlot.reset();
    m_placePlayerAt.reset();
    m_pendingEvents.clear();
    m_events.clear();
    m_input.releaseAll();
    m_input.endFrame();
    m_runner->setState(GameState::newGame(*m_setup.content));   // a new game
    beginScene(scene, sceneId);
}

void Game::stop()
{
    m_scene.reset();
    m_toDestroy.clear();
    m_nextScene.reset();
    m_loadSlot.reset();
    m_input.releaseAll();
    m_input.endFrame();
}

void Game::beginScene(Scene scene, AssetId id)
{
    m_sceneId = id;
    m_runner->state().scene = id;
    // Leave out what this playthrough removed for good (e.g. collected gems).
    for (const EntityId removed : std::vector<EntityId>(m_runner->state().removed(id).begin(),
                                                         m_runner->state().removed(id).end())) {
        scene.remove(removed);
    }
    m_sceneObjects.clear();
    m_textTemplates.clear();
    scene.forEach([&](const Entity& e, int) {
        m_sceneObjects.insert(e.id());
        if (const Text* text = e.get<Text>()) {
            m_textTemplates[e.id()] = text->text;
        }
    });
    m_triggers.clear();
    m_prompt.clear();
    m_scene = std::make_shared<Scene>(std::move(scene));

    // After loading a save: put the player where they stood.
    if (m_placePlayerAt) {
        for (const EntityId player : findAll<Interactor>()) {
            Vec2 local = *m_placePlayerAt;
            if (const Entity* parent = m_scene->find(m_scene->find(player)->parent())) {
                if (const auto inverse = m_scene->worldTransform(parent->id()).inverse()) {
                    local = inverse->apply(local);
                }
            }
            m_scene->edit<Transform>(player)->position = local;
            break;
        }
        m_placePlayerAt.reset();
    }
    updatePresentation();   // the dialogue box starts hidden, texts filled in
}

void Game::update(float dt)
{
    if (m_scene == nullptr) {
        return;
    }
    m_dt = std::clamp(dt, 0.0f, kMaxStep);
    m_time += static_cast<double>(m_dt);
    m_events.swap(m_pendingEvents);   // events sent last frame arrive now
    m_pendingEvents.clear();
    const bool talking = m_runner->inDialogue();
    if (talking) {
        handleDialogueInput();
    }
    runBehaviours();
    if (m_scene != nullptr) {
        runTriggers(talking);
        m_runner->update(m_dt);
        updatePresentation();
        finishFrame();
    }
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

// --- Behaviours ----------------------------------------------------------------------------------

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

// --- Triggers and dialogue -------------------------------------------------------------------------

void Game::handleDialogueInput()
{
    const auto& line = m_runner->dialogueLine();
    if (!line) {
        return;
    }
    // The player's interact key, Enter or Space go on; up and down pick a choice.
    Key interact = Key::E;
    for (const EntityId id : findAll<Interactor>()) {
        interact = toKey(m_scene->find(id)->get<Interactor>()->key);
        break;
    }
    const bool confirm = m_input.wasPressed(interact) || m_input.wasPressed(Key::Enter) || m_input.wasPressed(Key::Space);
    if (!line->choices.empty()) {
        const std::size_t count = line->choices.size();
        if (m_input.wasPressed(Key::Down) || m_input.wasPressed(Key::S)) {
            m_choice = (m_choice + 1) % count;
        }
        if (m_input.wasPressed(Key::Up) || m_input.wasPressed(Key::W)) {
            m_choice = (m_choice + count - 1) % count;
        }
        if (confirm) {
            m_runner->choose(std::min(m_choice, count - 1));
        }
    } else if (confirm) {
        m_runner->continueDialogue();
    }
}

void Game::runTriggers(bool talkedThisFrame)
{
    const std::vector<EntityId> players = findAll<Interactor>();
    const auto playerInside = [&](const Box& area) {
        for (const EntityId p : players) {
            const Entity* e = m_scene->find(p);
            if (e == nullptr || !isActiveInScene(*e)) {
                continue;
            }
            if (const auto box = colliderBox(p); box && box->overlaps(area)) {
                return true;
            }
        }
        return false;
    };
    Key interactKey = Key::E;
    if (!players.empty()) {
        interactKey = toKey(m_scene->find(players.front())->get<Interactor>()->key);
    }
    // A key press used by the dialogue this frame (e.g. to close it) must not
    // also start talking again.
    const bool canInteract = !m_runner->inDialogue() && !talkedThisFrame;
    bool interacted = false;
    m_prompt.clear();

    for (const EntityId id : findAll<Trigger>()) {
        const Entity* entity = m_scene->find(id);
        if (entity == nullptr || !isActiveInScene(*entity)) {
            continue;
        }
        const Trigger trigger = *entity->get<Trigger>();   // a copy: actions may change the scene
        TriggerState& state = m_triggers[id];
        if (trigger.once && (state.fired || (!m_sceneId.isNull() && m_runner->state().hasFired(m_sceneId, id)))) {
            continue;
        }
        bool fire = false;
        const auto area = colliderBox(id);
        switch (trigger.on) {
        case TriggerOn::SceneStart:
            fire = !state.started;
            state.started = true;
            break;
        case TriggerOn::Timer:
            state.elapsed += m_dt;
            if (state.elapsed >= trigger.delay) {
                state.elapsed -= std::max(trigger.delay, 0.001f);
                fire = true;
            }
            break;
        case TriggerOn::EnterArea:
        case TriggerOn::LeaveArea: {
            const bool inside = area && playerInside(*area);
            fire = trigger.on == TriggerOn::EnterArea ? (inside && !state.inside) : (!inside && state.inside);
            state.inside = inside;
            break;
        }
        case TriggerOn::Interact: {
            const bool inside = area && playerInside(*area);
            const bool allowed = !trigger.reaction.condition || m_runner->evaluate(*trigger.reaction.condition);
            if (inside && canInteract && allowed && m_prompt.empty() && !interacted) {
                m_prompt = trigger.prompt.empty() ? std::string()
                                                  : "[" + std::string(keyName(interactKey)) + "] " + trigger.prompt;
                if (m_input.wasPressed(interactKey)) {
                    fire = true;
                    interacted = true;   // one interaction per key press
                }
            }
            break;
        }
        case TriggerOn::GameEvent:
            fire = !trigger.event.empty() && wasEmitted(trigger.event);
            break;
        case TriggerOn::ConditionMet: {
            const bool now = trigger.reaction.condition && m_runner->evaluate(*trigger.reaction.condition);
            fire = now && !state.conditionWas;
            state.conditionWas = now;
            break;
        }
        }
        if (!fire) {
            continue;
        }
        if (trigger.on != TriggerOn::ConditionMet && trigger.reaction.condition
            && !m_runner->evaluate(*trigger.reaction.condition)) {
            continue;
        }
        if (trigger.once) {
            state.fired = true;
            if (!m_sceneId.isNull()) {
                m_runner->state().fired(m_sceneId).insert(id);
            }
        }
        m_runner->run(trigger.reaction.actions, id);
        if (m_runner->inDialogue()) {
            m_prompt.clear();
        }
    }
}

void Game::updatePresentation()
{
    if (m_scene == nullptr) {
        return;
    }
    const auto& line = m_runner->dialogueLine();
    // A new line: the first choice is selected again.
    const std::string lineKey = line ? line->dialogue.toString() + "/" + line->node : std::string();
    if (lineKey != m_shownLine) {
        m_shownLine = lineKey;
        m_choice = 0;
    }

    std::string marker = "> ";
    for (const EntityId id : findAll<DialogueView>()) {
        const Entity* e = m_scene->find(id);
        marker = e->get<DialogueView>()->marker;
        if (e->isActive() != line.has_value()) {
            m_scene->setActive(id, line.has_value());
        }
    }

    for (const EntityId id : findAll<Text>()) {
        const Text& text = *m_scene->find(id)->get<Text>();
        const auto known = m_textTemplates.find(id);
        if (known == m_textTemplates.end()) {
            m_textTemplates[id] = text.text;   // made while playing: its text is the template
        }
        const std::string& written = m_textTemplates[id];
        std::string shown;
        Color color = text.color;
        switch (text.source) {
        case TextSource::Text: shown = m_runner->fillIn(written); break;
        case TextSource::DialogueSpeaker:
            if (line) {
                shown = line->speakerName;
                color = line->speakerColor;
            }
            break;
        case TextSource::DialogueLine: shown = line ? line->text : std::string(); break;
        case TextSource::DialogueChoices: shown = dialogueChoicesText(marker); break;
        case TextSource::InteractionPrompt: shown = m_prompt; break;
        case TextSource::QuestLog: shown = questLogText(); break;
        }
        // Only a real change redraws (changing text rebuilds the drawing).
        if (shown != text.text || color != text.color) {
            Text* edit = m_scene->edit<Text>(id);
            edit->text = std::move(shown);
            edit->color = color;
        }
    }
}

std::string Game::dialogueChoicesText(std::string_view marker) const
{
    const auto& line = m_runner->dialogueLine();
    if (!line || line->choices.empty()) {
        return {};
    }
    const std::string blank(marker.size(), ' ');
    std::string out;
    for (std::size_t i = 0; i < line->choices.size(); ++i) {
        out += (i == 0 ? "" : "\n") + (i == m_choice ? std::string(marker) : blank) + line->choices[i].text;
    }
    return out;
}

std::string Game::questLogText() const
{
    std::string out;
    const ContentLibrary& content = *m_setup.content;
    for (const AssetId id : content.questIds()) {
        if (m_runner->state().questState(id) != QuestState::Active) {
            continue;
        }
        const Quest& quest = *content.quest(id);
        out += (out.empty() ? "" : "\n") + m_runner->fillIn(quest.name);
        for (const Objective& o : quest.objectives) {
            const bool done = m_runner->state().isObjectiveDone(id, o.id);
            out += std::string("\n") + (done ? "  [x] " : "  [ ] ") + m_runner->fillIn(o.text.empty() ? o.id : o.text)
                   + (o.optional ? " (optional)" : "");
        }
    }
    return out;
}

// --- End of frame ----------------------------------------------------------------------------------

void Game::finishFrame()
{
    for (const EntityId id : m_toDestroy) {
        if (!m_scene->contains(id)) {
            continue;
        }
        // Objects the scene came with stay removed for this playthrough.
        if (!m_sceneId.isNull() && m_sceneObjects.count(id) > 0) {
            m_runner->state().removed(m_sceneId).insert(id);
        }
        m_scene->remove(id);
    }
    m_toDestroy.clear();

    if (m_loadSlot) {
        const std::string slot = *m_loadSlot;
        m_loadSlot.reset();
        m_nextScene.reset();
        const auto text = m_setup.saves != nullptr ? m_setup.saves->read(slot) : std::nullopt;
        if (!text) {
            m_log.error(kCategory, "There is no save in slot '" + slot + "'.");
            return;
        }
        SaveLoadResult loaded = parseSave(*text, *m_setup.content);
        for (const FileIssue& issue : loaded.issues) {
            (issue.isError() ? m_log.error(kCategory, issue.toString()) : m_log.warning(kCategory, issue.toString()));
        }
        if (!loaded.ok()) {
            return;
        }
        std::string error = "this program cannot load scenes";
        std::optional<Scene> scene = m_setup.loadScene && !loaded.state->scene.isNull()
                                         ? m_setup.loadScene(loaded.state->scene, error)
                                         : std::nullopt;
        if (!scene) {
            m_log.error(kCategory, "Could not load slot '" + slot + "': its scene could not be opened (" + error + ").");
            return;
        }
        m_placePlayerAt = loaded.state->playerPosition;
        const AssetId sceneId = loaded.state->scene;
        m_runner->setState(std::move(*loaded.state));
        beginScene(std::move(*scene), sceneId);
        m_log.info(kCategory, "Loaded slot '" + slot + "'.");
        return;
    }

    if (!m_nextScene) {
        return;
    }
    const AssetId next = *m_nextScene;
    m_nextScene.reset();
    std::string error = "this program cannot load scenes";
    std::optional<Scene> loaded = m_setup.loadScene ? m_setup.loadScene(next, error) : std::nullopt;
    if (!loaded) {
        m_log.error(kCategory, "Could not change to scene " + next.toString() + ": " + error);
        return;
    }
    m_log.info(kCategory, "Changed to scene '" + loaded->name() + "'.");
    beginScene(std::move(*loaded), next);
}

// --- Objects -----------------------------------------------------------------------------------------

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

void Game::setActive(EntityId entity, bool active)
{
    if (m_scene != nullptr && !m_scene->setActive(entity, active) && !m_scene->contains(entity)) {
        m_log.warning(kCategory, "Cannot switch object " + entity.toString() + " on or off: it is not in this scene.");
    }
}

void Game::emit(std::string_view event)
{
    m_pendingEvents.emplace_back(event);
}

bool Game::wasEmitted(std::string_view event) const
{
    return std::find(m_events.begin(), m_events.end(), event) != m_events.end();
}

bool Game::save(std::string_view slot)
{
    if (m_setup.saves == nullptr) {
        m_log.warning(kCategory, "Saving is not available here; slot '" + std::string(slot) + "' was not saved.");
        return false;
    }
    if (m_sceneId.isNull()) {
        m_log.warning(kCategory, "This scene is not saved in the project yet, so the game cannot be saved.");
        return false;
    }
    GameState& state = m_runner->state();
    state.scene = m_sceneId;
    state.playerPosition.reset();
    for (const EntityId player : findAll<Interactor>()) {
        state.playerPosition = worldPosition(player);
        break;
    }
    if (const Status s = m_setup.saves->write(slot, serializeSave(state, m_setup.gameName)); !s) {
        m_log.error(kCategory, s.message);
        return false;
    }
    m_log.info(kCategory, "Saved to slot '" + std::string(slot) + "'.");
    return true;
}

void Game::load(std::string_view slot)
{
    m_loadSlot = std::string(slot);
}

// --- Collision ---------------------------------------------------------------------------------------

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
    return m_scene != nullptr && lantern::isBlocked(*m_scene, m_setup.assets, box, ignore);
}

MoveResult Game::move(EntityId id, Vec2 delta)
{
    MoveResult result;
    const Entity* entity = m_scene != nullptr ? m_scene->find(id) : nullptr;
    if (entity == nullptr) {
        return result;
    }
    if (const auto box = colliderBox(id)) {
        result = sweepBox(*m_scene, m_setup.assets, *box, delta, id);
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
