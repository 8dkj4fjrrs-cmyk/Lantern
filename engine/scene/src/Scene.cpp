#include "lantern/scene/Scene.hpp"

#include <algorithm>
#include <cmath>

namespace lantern {

// --- Entity ----------------------------------------------------------------------------

const Component* Entity::component(std::string_view typeId) const
{
    for (const auto& c : m_components) {
        if (c->type().id == typeId) {
            return c.get();
        }
        if (const auto* unknown = dynamic_cast<const UnknownComponent*>(c.get());
            unknown != nullptr && unknown->originalTypeId() == typeId) {
            return c.get();
        }
    }
    return nullptr;
}

Component* Entity::component(std::string_view typeId)
{
    return const_cast<Component*>(static_cast<const Entity&>(*this).component(typeId));
}

// --- Scene: construction ---------------------------------------------------------------

Scene::Scene(std::string name)
    : m_name(std::move(name))
{
}

Scene::Scene(std::string name, EntityIdGenerator generator)
    : m_name(std::move(name))
    , m_ids(std::move(generator))
{
}

Scene::Scene(const Scene& other)
    : m_name(other.m_name)
    , m_roots(other.m_roots)
    , m_ids(other.m_ids)
    , m_revision(other.m_revision)
{
    for (const auto& [id, entity] : other.m_entities) {
        auto copy = std::make_unique<Entity>();
        copy->m_id = entity->m_id;
        copy->m_name = entity->m_name;
        copy->m_active = entity->m_active;
        copy->m_parent = entity->m_parent;
        copy->m_children = entity->m_children;
        for (const auto& c : entity->m_components) {
            copy->m_components.push_back(c->clone());
        }
        m_entities.emplace(id, std::move(copy));
    }
}

Scene& Scene::operator=(const Scene& other)
{
    if (this != &other) {
        Scene copy(other);
        *this = std::move(copy);
    }
    return *this;
}

void Scene::setName(std::string name)
{
    m_name = std::move(name);
    ++m_revision;
}

// --- Reading ---------------------------------------------------------------------------

const Entity* Scene::find(EntityId id) const
{
    const auto it = m_entities.find(id);
    return it == m_entities.end() ? nullptr : it->second.get();
}

Entity* Scene::findMutable(EntityId id)
{
    const auto it = m_entities.find(id);
    return it == m_entities.end() ? nullptr : it->second.get();
}

const std::vector<EntityId>& Scene::childrenOf(EntityId parent) const
{
    if (parent.isNull()) {
        return m_roots;
    }
    static const std::vector<EntityId> none;
    const Entity* p = find(parent);
    return p != nullptr ? p->m_children : none;
}

std::vector<EntityId>* Scene::siblingsOf(EntityId parent)
{
    if (parent.isNull()) {
        return &m_roots;
    }
    Entity* p = findMutable(parent);
    return p != nullptr ? &p->m_children : nullptr;
}

std::optional<std::size_t> Scene::indexOf(EntityId id) const
{
    const Entity* e = find(id);
    if (e == nullptr) {
        return std::nullopt;
    }
    const auto& siblings = childrenOf(e->m_parent);
    const auto it = std::find(siblings.begin(), siblings.end(), id);
    if (it == siblings.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(it - siblings.begin());
}

bool Scene::isAncestorOrSelf(EntityId ancestor, EntityId id) const
{
    for (const Entity* e = find(id); e != nullptr; e = find(e->m_parent)) {
        if (e->m_id == ancestor) {
            return true;
        }
    }
    return false;
}

void Scene::forEach(const std::function<void(const Entity&, int depth)>& visit) const
{
    // Iterative depth-first walk (no recursion limit for deep trees).
    std::vector<std::pair<EntityId, int>> stack;
    for (auto it = m_roots.rbegin(); it != m_roots.rend(); ++it) {
        stack.emplace_back(*it, 0);
    }
    while (!stack.empty()) {
        const auto [id, depth] = stack.back();
        stack.pop_back();
        const Entity* e = find(id);
        if (e == nullptr) {
            continue;
        }
        visit(*e, depth);
        for (auto it = e->m_children.rbegin(); it != e->m_children.rend(); ++it) {
            stack.emplace_back(*it, depth + 1);
        }
    }
}

// --- Changing ---------------------------------------------------------------------------

EntityId Scene::newId()
{
    EntityId id = m_ids.next();
    while (contains(id)) {
        id = m_ids.next();
    }
    return id;
}

Entity* Scene::create(std::string name, EntityId parent, std::optional<std::size_t> index, EntityId id)
{
    std::vector<EntityId>* siblings = siblingsOf(parent);
    if (siblings == nullptr) {
        return nullptr;
    }
    if (id.isNull()) {
        id = newId();
    } else if (contains(id)) {
        return nullptr;
    }
    auto entity = std::make_unique<Entity>();
    entity->m_id = id;
    entity->m_name = std::move(name);
    entity->m_parent = parent;
    entity->m_components.push_back(std::make_unique<Transform>());
    Entity* raw = entity.get();
    m_entities.emplace(id, std::move(entity));
    const std::size_t at = std::min(index.value_or(siblings->size()), siblings->size());
    siblings->insert(siblings->begin() + static_cast<std::ptrdiff_t>(at), id);
    ++m_revision;
    return raw;
}

void Scene::eraseSubtree(EntityId id)
{
    std::vector<EntityId> pending{id};
    while (!pending.empty()) {
        const EntityId current = pending.back();
        pending.pop_back();
        if (const Entity* e = find(current)) {
            pending.insert(pending.end(), e->m_children.begin(), e->m_children.end());
        }
        m_entities.erase(current);
    }
}

bool Scene::remove(EntityId id)
{
    const Entity* e = find(id);
    if (e == nullptr) {
        return false;
    }
    if (std::vector<EntityId>* siblings = siblingsOf(e->m_parent)) {
        siblings->erase(std::remove(siblings->begin(), siblings->end(), id), siblings->end());
    }
    eraseSubtree(id);
    ++m_revision;
    return true;
}

bool Scene::rename(EntityId id, std::string name)
{
    Entity* e = findMutable(id);
    if (e == nullptr) {
        return false;
    }
    e->m_name = std::move(name);
    ++m_revision;
    return true;
}

bool Scene::setActive(EntityId id, bool active)
{
    Entity* e = findMutable(id);
    if (e == nullptr) {
        return false;
    }
    e->m_active = active;
    ++m_revision;
    return true;
}

bool Scene::move(EntityId id, EntityId newParent, std::size_t index)
{
    Entity* e = findMutable(id);
    if (e == nullptr || (!newParent.isNull() && isAncestorOrSelf(id, newParent))) {
        return false;
    }
    std::vector<EntityId>* target = siblingsOf(newParent);
    if (target == nullptr) {
        return false;
    }
    std::vector<EntityId>* source = siblingsOf(e->m_parent);
    if (source != nullptr) {
        source->erase(std::remove(source->begin(), source->end(), id), source->end());
    }
    e->m_parent = newParent;
    const std::size_t at = std::min(index, target->size());
    target->insert(target->begin() + static_cast<std::ptrdiff_t>(at), id);
    ++m_revision;
    return true;
}

bool Scene::setProperty(EntityId id, std::string_view componentType, std::string_view key, const PropertyValue& value)
{
    Entity* e = findMutable(id);
    if (e == nullptr) {
        return false;
    }
    Component* c = e->component(componentType);
    if (c == nullptr || !c->set(key, value)) {
        return false;
    }
    ++m_revision;
    return true;
}

Component* Scene::addComponent(EntityId id, std::unique_ptr<Component> component, std::optional<std::size_t> index)
{
    Entity* e = findMutable(id);
    if (e == nullptr || component == nullptr) {
        return nullptr;
    }
    const std::string& typeId = dynamic_cast<const UnknownComponent*>(component.get()) != nullptr
                                    ? static_cast<const UnknownComponent&>(*component).originalTypeId()
                                    : component->type().id;
    if (e->component(typeId) != nullptr) {
        return nullptr;
    }
    Component* raw = component.get();
    const std::size_t at = std::min(index.value_or(e->m_components.size()), e->m_components.size());
    e->m_components.insert(e->m_components.begin() + static_cast<std::ptrdiff_t>(at), std::move(component));
    ++m_revision;
    return raw;
}

std::unique_ptr<Component> Scene::removeComponent(EntityId id, std::string_view componentType)
{
    Entity* e = findMutable(id);
    if (e == nullptr) {
        return nullptr;
    }
    for (auto it = e->m_components.begin(); it != e->m_components.end(); ++it) {
        const Component& c = **it;
        const auto* unknown = dynamic_cast<const UnknownComponent*>(&c);
        const bool match = unknown != nullptr ? unknown->originalTypeId() == componentType : c.type().id == componentType;
        if (!match) {
            continue;
        }
        if (c.type().required) {
            return nullptr;
        }
        std::unique_ptr<Component> removed = std::move(*it);
        e->m_components.erase(it);
        ++m_revision;
        return removed;
    }
    return nullptr;
}

std::optional<std::size_t> Scene::componentIndex(EntityId id, std::string_view componentType) const
{
    const Entity* e = find(id);
    if (e == nullptr) {
        return std::nullopt;
    }
    const Component* c = e->component(componentType);
    for (std::size_t i = 0; i < e->m_components.size(); ++i) {
        if (e->m_components[i].get() == c && c != nullptr) {
            return i;
        }
    }
    return std::nullopt;
}

// --- Snapshots ---------------------------------------------------------------------------

void Scene::captureInto(const Entity& entity, EntitySnapshot& out) const
{
    EntitySnapshot::Item item;
    item.id = entity.m_id;
    item.parent = entity.m_parent;
    item.name = entity.m_name;
    item.active = entity.m_active;
    for (const auto& c : entity.m_components) {
        item.components.push_back(c->clone());
    }
    out.items.push_back(std::move(item));
    for (EntityId child : entity.m_children) {
        if (const Entity* c = find(child)) {
            captureInto(*c, out);
        }
    }
}

EntitySnapshot Scene::capture(EntityId id) const
{
    EntitySnapshot snapshot;
    if (const Entity* e = find(id)) {
        captureInto(*e, snapshot);
        snapshot.index = indexOf(id).value_or(0);
    }
    return snapshot;
}

bool Scene::restore(const EntitySnapshot& snapshot, EntityId parent, std::size_t index)
{
    if (snapshot.empty() || siblingsOf(parent) == nullptr) {
        return false;
    }
    for (const auto& item : snapshot.items) {
        if (contains(item.id)) {
            return false;
        }
    }
    for (std::size_t i = 0; i < snapshot.items.size(); ++i) {
        const auto& item = snapshot.items[i];
        auto entity = std::make_unique<Entity>();
        entity->m_id = item.id;
        entity->m_name = item.name;
        entity->m_active = item.active;
        entity->m_parent = i == 0 ? parent : item.parent;
        for (const auto& c : item.components) {
            entity->m_components.push_back(c->clone());
        }
        if (entity->get<Transform>() == nullptr) {
            entity->m_components.insert(entity->m_components.begin(), std::make_unique<Transform>());
        }
        const EntityId myParent = entity->m_parent;
        m_entities.emplace(item.id, std::move(entity));
        std::vector<EntityId>* siblings = siblingsOf(myParent);
        if (siblings == nullptr) {
            siblings = &m_roots; // a broken snapshot: keep the entity rather than lose it
            findMutable(item.id)->m_parent = {};
        }
        const std::size_t at = i == 0 ? std::min(index, siblings->size()) : siblings->size();
        siblings->insert(siblings->begin() + static_cast<std::ptrdiff_t>(at), item.id);
    }
    ++m_revision;
    return true;
}

EntitySnapshot Scene::withNewIds(const EntitySnapshot& snapshot)
{
    EntitySnapshot copy;
    copy.index = snapshot.index;
    std::unordered_map<EntityId, EntityId> remap;
    for (const auto& item : snapshot.items) {
        EntityId fresh = newId();
        // Also avoid IDs given out earlier in this copy.
        while (std::any_of(remap.begin(), remap.end(), [&](const auto& kv) { return kv.second == fresh; })) {
            fresh = newId();
        }
        remap[item.id] = fresh;
    }
    for (const auto& item : snapshot.items) {
        EntitySnapshot::Item c;
        c.id = remap[item.id];
        const auto parent = remap.find(item.parent);
        c.parent = parent != remap.end() ? parent->second : item.parent;
        c.name = item.name;
        c.active = item.active;
        for (const auto& comp : item.components) {
            c.components.push_back(comp->clone());
        }
        copy.items.push_back(std::move(c));
    }
    return copy;
}

// --- Geometry ------------------------------------------------------------------------------

Affine2 Scene::worldTransform(EntityId id) const
{
    Affine2 result;
    for (const Entity* e = find(id); e != nullptr; e = find(e->m_parent)) {
        if (const Transform* t = e->get<Transform>()) {
            result = t->local() * result;
        }
    }
    return result;
}

std::optional<LocalBounds> localBounds(const Entity& entity)
{
    if (const Shape* shape = entity.get<Shape>()) {
        return LocalBounds{shape->size, shape->kind == ShapeKind::Ellipse};
    }
    if (const Sprite* sprite = entity.get<Sprite>()) {
        return LocalBounds{sprite->size, false};
    }
    return std::nullopt;
}

std::optional<std::array<Vec2, 4>> Scene::worldCorners(EntityId id) const
{
    const Entity* e = find(id);
    const auto bounds = e != nullptr ? localBounds(*e) : std::nullopt;
    if (!bounds) {
        return std::nullopt;
    }
    const Affine2 world = worldTransform(id);
    const float hw = bounds->size.x * 0.5f;
    const float hh = bounds->size.y * 0.5f;
    return std::array<Vec2, 4>{world.apply({-hw, -hh}), world.apply({hw, -hh}), world.apply({hw, hh}),
                               world.apply({-hw, hh})};
}

bool Scene::isEffectivelyActive(const Entity& entity) const
{
    for (const Entity* e = &entity; e != nullptr; e = find(e->m_parent)) {
        if (!e->m_active) {
            return false;
        }
    }
    return true;
}

EntityId Scene::hitTest(Vec2 worldPoint) const
{
    // Everything in drawing order; the last hit is the one on top.
    EntityId hit;
    forEach([&](const Entity& e, int) {
        const auto bounds = localBounds(e);
        if (!bounds || bounds->size.x <= 0.0f || bounds->size.y <= 0.0f || !isEffectivelyActive(e)) {
            return;
        }
        const auto inverse = worldTransform(e.m_id).inverse();
        if (!inverse) {
            return;
        }
        const Vec2 p = inverse->apply(worldPoint);
        const float hw = bounds->size.x * 0.5f;
        const float hh = bounds->size.y * 0.5f;
        bool inside = false;
        if (!bounds->ellipse) {
            inside = std::abs(p.x) <= hw && std::abs(p.y) <= hh;
        } else {
            const float nx = p.x / hw;
            const float ny = p.y / hh;
            inside = nx * nx + ny * ny <= 1.0f;
        }
        if (inside) {
            hit = e.m_id;
        }
    });
    return hit;
}

// --- Start scene -------------------------------------------------------------------------------

namespace {

Entity* shapeEntity(Scene& scene, const char* name, EntityId parent, Vec2 position, ShapeKind kind, Vec2 size,
                    Color color, bool filled = true, float lineWidth = 2.0f)
{
    Entity* e = scene.create(name, parent);
    scene.setProperty(e->id(), "lantern.Transform", "position", position);
    auto shape = std::make_unique<Shape>();
    shape->kind = kind;
    shape->size = size;
    shape->color = color;
    shape->filled = filled;
    shape->lineWidth = lineWidth;
    scene.addComponent(e->id(), std::move(shape));
    return e;
}

} // namespace

Scene makeStartScene(float worldWidth, float worldHeight)
{
    Scene scene("Start Scene");
    const Color glow{255, 184, 77, 18};
    const Color glass{255, 184, 77, 89};
    const Color flame{255, 184, 77, 255};
    const Color core{255, 224, 140, 255};
    const Color frame{56, 51, 61, 255};

    shapeEntity(scene, "Background", {}, {worldWidth * 0.5f, worldHeight * 0.5f}, ShapeKind::Rectangle,
                {worldWidth, worldHeight}, Color{15, 18, 28, 255});

    Entity* lantern = scene.create("Lantern");
    scene.setProperty(lantern->id(), "lantern.Transform", "position", Vec2{worldWidth * 0.5f, worldHeight * 0.5f});

    Entity* halo = scene.create("Glow", lantern->id());
    int ring = 1;
    for (float radius : {220.0f, 170.0f, 130.0f, 100.0f, 80.0f, 65.0f}) {
        const std::string name = "Glow " + std::to_string(ring++);
        shapeEntity(scene, name.c_str(), halo->id(), {}, ShapeKind::Ellipse, {radius * 2.0f, radius * 2.0f}, glow);
    }
    shapeEntity(scene, "Glass", lantern->id(), {}, ShapeKind::Rectangle, {76.0f, 110.0f}, glass);
    Entity* fire = shapeEntity(scene, "Flame", lantern->id(), {0.0f, -8.0f}, ShapeKind::Ellipse, {44.0f, 44.0f}, flame);
    shapeEntity(scene, "Core", fire->id(), {0.0f, 4.0f}, ShapeKind::Ellipse, {24.0f, 24.0f}, core);

    Entity* body = scene.create("Frame", lantern->id());
    shapeEntity(scene, "Top", body->id(), {0.0f, 61.5f}, ShapeKind::Rectangle, {96.0f, 13.0f}, frame);
    shapeEntity(scene, "Bottom", body->id(), {0.0f, -61.5f}, ShapeKind::Rectangle, {96.0f, 13.0f}, frame);
    shapeEntity(scene, "Left Bar", body->id(), {-39.0f, 0.0f}, ShapeKind::Rectangle, {6.0f, 110.0f}, frame);
    shapeEntity(scene, "Right Bar", body->id(), {39.0f, 0.0f}, ShapeKind::Rectangle, {6.0f, 110.0f}, frame);
    shapeEntity(scene, "Handle", body->id(), {0.0f, 82.0f}, ShapeKind::Ellipse, {36.0f, 36.0f}, frame, false, 3.0f);
    return scene;
}

} // namespace lantern
