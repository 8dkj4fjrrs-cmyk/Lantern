#include "lantern/scene/Component.hpp"
#include "lantern/scene/Components.hpp"

#include <algorithm>
#include <cmath>

namespace lantern {

namespace {

float clampFloat(float v, const PropertyOptions& o)
{
    if (!std::isfinite(v)) {
        v = 0.0f;
    }
    if (o.min) {
        v = std::max(v, *o.min);
    }
    if (o.max) {
        v = std::min(v, *o.max);
    }
    return v;
}

std::optional<float> asNumber(const PropertyValue& value)
{
    if (const float* f = std::get_if<float>(&value)) {
        return *f;
    }
    if (const int* i = std::get_if<int>(&value)) {
        return static_cast<float>(*i);
    }
    return std::nullopt;
}

} // namespace

std::optional<PropertyValue> PropertyInfo::coerce(const PropertyValue& value) const
{
    switch (kind) {
    case PropertyKind::Bool:
        if (const bool* b = std::get_if<bool>(&value)) {
            return *b;
        }
        return std::nullopt;
    case PropertyKind::Float:
        if (const auto n = asNumber(value)) {
            return clampFloat(*n, options);
        }
        return std::nullopt;
    case PropertyKind::Int:
        if (const auto n = asNumber(value)) {
            return static_cast<int>(std::lround(clampFloat(*n, options)));
        }
        return std::nullopt;
    case PropertyKind::Vec2:
        if (const Vec2* v = std::get_if<Vec2>(&value)) {
            return Vec2{clampFloat(v->x, options), clampFloat(v->y, options)};
        }
        return std::nullopt;
    case PropertyKind::Color:
        if (const Color* c = std::get_if<Color>(&value)) {
            return *c;
        }
        return std::nullopt;
    case PropertyKind::String:
        if (const std::string* s = std::get_if<std::string>(&value)) {
            return *s;
        }
        return std::nullopt;
    case PropertyKind::Choice:
        if (const int* i = std::get_if<int>(&value)) {
            if (*i >= 0 && static_cast<std::size_t>(*i) < options.choices.size()) {
                return *i;
            }
        }
        return std::nullopt;
    }
    return std::nullopt;
}

const PropertyInfo* ComponentType::findProperty(std::string_view key) const
{
    for (const PropertyInfo& p : properties) {
        if (p.key == key) {
            return &p;
        }
    }
    return nullptr;
}

std::optional<PropertyValue> Component::get(std::string_view key) const
{
    const PropertyInfo* p = type().findProperty(key);
    if (p == nullptr || !p->get) {
        return std::nullopt;
    }
    return p->get(*this);
}

bool Component::set(std::string_view key, const PropertyValue& value)
{
    const PropertyInfo* p = type().findProperty(key);
    if (p == nullptr || !p->set) {
        return false;
    }
    const auto coerced = p->coerce(value);
    if (!coerced) {
        return false;
    }
    p->set(*this, *coerced);
    return true;
}

const ComponentType& UnknownComponent::type() const
{
    static const ComponentType unknown = [] {
        ComponentType t;
        t.id = std::string(kTypeId);
        t.displayName = "Unknown Component";
        t.description = "A component this version of Lantern does not recognise. Its data is kept "
                        "unchanged and saved again with the scene.";
        return t;
    }();
    return unknown;
}

// --- Registry ------------------------------------------------------------------

ComponentRegistry ComponentRegistry::withBuiltins()
{
    ComponentRegistry registry;
    static_cast<void>(registry.add(Transform::componentType()));
    static_cast<void>(registry.add(Shape::componentType()));
    return registry;
}

Status ComponentRegistry::add(const ComponentType& type)
{
    if (type.id.empty()) {
        return Status::failure("A component type needs an id.");
    }
    if (find(type.id) != nullptr) {
        return Status::failure("The component type '" + type.id + "' is already registered.");
    }
    m_types.push_back(&type);
    return Status::success();
}

const ComponentType* ComponentRegistry::find(std::string_view id) const
{
    for (const ComponentType* t : m_types) {
        if (t->id == id) {
            return t;
        }
    }
    return nullptr;
}

// --- Built-in components ------------------------------------------------------------

const ComponentType& Transform::componentType()
{
    static const ComponentType type =
        ComponentTypeBuilder<Transform>("lantern.Transform", "Transform")
            .description("Where the object is, relative to its parent. Position is in points from the "
                         "parent's origin (y points up), rotation in degrees clockwise, and scale "
                         "multiplies the size (1 is normal size).")
            .required()
            .property("position", "Position", &Transform::position, {.step = 1.0f})
            .property("rotation", "Rotation", &Transform::rotation,
                      {.step = 1.0f, .help = "Degrees, clockwise.", .unit = "\u00B0"})
            .property("scale", "Scale", &Transform::scale, {.step = 0.1f})
            .build();
    return type;
}

const ComponentType& Shape::componentType()
{
    static const ComponentType type =
        ComponentTypeBuilder<Shape>("lantern.Shape", "Shape")
            .description("Draws a rectangle or an ellipse centred on the object, filled or as an outline.")
            .property("kind", "Kind", &Shape::kind, {.choices = {"rectangle", "ellipse"}})
            .property("size", "Size", &Shape::size, {.min = 0.0f, .step = 1.0f})
            .property("color", "Color", &Shape::color)
            .property("filled", "Filled", &Shape::filled)
            .property("lineWidth", "Line Width", &Shape::lineWidth,
                      {.min = 0.0f, .max = 100.0f, .step = 0.5f, .help = "Outline thickness, when not filled.", .unit = "pt"})
            .build();
    return type;
}

} // namespace lantern
