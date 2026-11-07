#include "rolo/gameplay/Components.hpp"

#include "rolo/gameplay/BuiltinBehaviours.hpp"

namespace rolo {

Key toKey(InteractKey key)
{
    switch (key) {
    case InteractKey::E: return Key::E;
    case InteractKey::Space: return Key::Space;
    case InteractKey::Enter: return Key::Enter;
    case InteractKey::Z: return Key::Z;
    }
    return Key::E;
}

const ComponentType& Interactor::componentType()
{
    static const ComponentType type =
        ComponentTypeBuilder<Interactor>("rolo.Interactor", "Interactor")
            .category("Gameplay")
            .description("Marks the player: Triggers notice it walking into their area and pressing the interact "
                         "key. Needs a Collider.")
            .property("key", "Interact Key", &Interactor::key,
                      {.choices = {"E", "Space", "Enter", "Z"}, .help = "Pressed to talk, open, pick up..."})
            .build();
    return type;
}

std::string Trigger::saveData() const
{
    return serializeReaction(reaction);
}

std::string Trigger::loadData(std::string_view json)
{
    std::string error;
    if (auto r = parseReaction(json, &error)) {
        reaction = std::move(*r);
        return {};
    }
    return "The trigger's condition or actions could not be read (" + error + "); they are left empty.";
}

const ComponentType& Trigger::componentType()
{
    static const ComponentType type =
        ComponentTypeBuilder<Trigger>("rolo.Trigger", "Trigger")
            .category("Gameplay")
            .description("When something happens, runs actions (if its condition is true). Interact and area "
                         "triggers use this object's Collider as the area: make it not solid.")
            .property("on", "When", &Trigger::on,
                      {.choices = {"interact", "enterArea", "leaveArea", "sceneStart", "timer", "gameEvent", "conditionMet"},
                       .help = "Interact: the player presses the interact key in the area. Enter/Leave Area: the "
                               "player walks in or out. Scene Start, Timer (after Delay), Game Event (by name), "
                               "Condition Met (when the condition becomes true)."})
            .property("prompt", "Prompt", &Trigger::prompt, {.help = "Interact: shown to the player, e.g. \"Talk\"."})
            .property("event", "Event", &Trigger::event, {.help = "Game Event: the name of the event."})
            .property("delay", "Delay", &Trigger::delay,
                      {.min = 0.0f, .step = 0.5f, .help = "Timer: seconds after the scene starts (and between repeats).", .unit = "s"})
            .property("once", "Once", &Trigger::once,
                      {.help = "Run only once per playthrough. Remembered in save games."})
            .build();
    return type;
}

const ComponentType& DialogueView::componentType()
{
    static const ComponentType type =
        ComponentTypeBuilder<DialogueView>("rolo.DialogueView", "Dialogue View")
            .category("Gameplay")
            .description("Shown only while a dialogue is going on: put it on the dialogue box. Text objects inside "
                         "it with a dialogue Source show the speaker, the line and the choices.")
            .property("marker", "Choice Marker", &DialogueView::marker,
                      {.help = "Written in front of the selected choice."})
            .build();
    return type;
}

void registerGameplayComponents(ComponentRegistry& registry)
{
    static_cast<void>(registry.add(Interactor::componentType()));
    static_cast<void>(registry.add(Trigger::componentType()));
    static_cast<void>(registry.add(DialogueView::componentType()));
    registerBuiltinBehaviours(registry);
}

} // namespace rolo
