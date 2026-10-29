#pragma once

// EntityId: the permanent identity of an entity.
//
// Names can repeat and change, and memory addresses change every run, so
// scenes (and later: prefabs, save games, quests) refer to entities by ID.
// An ID is a random 64-bit number, written in files as 16 hex digits, e.g.
// "3f9c2a7d10b84e65". Random IDs stay unique when scenes are merged or
// edited by several people, without a central counter.

#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <string_view>

namespace lantern {

class EntityId {
public:
    constexpr EntityId() = default;
    constexpr explicit EntityId(std::uint64_t value)
        : m_value(value)
    {
    }

    /// 0 means "no entity" (e.g. the parent of a top-level entity).
    constexpr bool isNull() const noexcept { return m_value == 0; }
    constexpr explicit operator bool() const noexcept { return m_value != 0; }
    constexpr std::uint64_t value() const noexcept { return m_value; }

    /// 16 lowercase hex digits; empty for the null ID.
    std::string toString() const;
    /// Accepts exactly 16 hex digits (any case). Nothing for anything else, or for zero.
    static std::optional<EntityId> parse(std::string_view text);

    friend constexpr bool operator==(EntityId, EntityId) = default;
    friend constexpr auto operator<=>(EntityId, EntityId) = default;

private:
    std::uint64_t m_value = 0;
};

/// AssetId: the permanent identity of an asset (an image, a tileset, a
/// sound...). Stored in the asset's ".meta" file next to it, and used by
/// scenes to refer to the asset, so renaming or moving the file in the
/// editor never breaks a reference. Same form as an EntityId.
class AssetId {
public:
    constexpr AssetId() = default;
    constexpr explicit AssetId(std::uint64_t value)
        : m_value(value)
    {
    }
    /// 0 means "no asset" (e.g. a Sprite with no image chosen yet).
    constexpr bool isNull() const noexcept { return m_value == 0; }
    constexpr explicit operator bool() const noexcept { return m_value != 0; }
    constexpr std::uint64_t value() const noexcept { return m_value; }
    std::string toString() const;
    static std::optional<AssetId> parse(std::string_view text);
    friend constexpr bool operator==(AssetId, AssetId) = default;
    friend constexpr auto operator<=>(AssetId, AssetId) = default;

private:
    std::uint64_t m_value = 0;
};

/// Makes new random IDs. Each Scene owns one (no global random state).
class EntityIdGenerator {
public:
    /// Seeded from the operating system's random source.
    EntityIdGenerator();
    /// A fixed seed: the same IDs every time (for tests).
    explicit EntityIdGenerator(std::uint64_t seed);

    EntityId next();
    /// A random non-zero 64-bit value (for other kinds of ID, e.g. AssetId).
    std::uint64_t nextValue();

private:
    std::mt19937_64 m_engine;
};

} // namespace lantern

template <>
struct std::hash<lantern::AssetId> {
    std::size_t operator()(lantern::AssetId id) const noexcept { return std::hash<std::uint64_t>{}(id.value()); }
};

template <>
struct std::hash<lantern::EntityId> {
    std::size_t operator()(lantern::EntityId id) const noexcept { return std::hash<std::uint64_t>{}(id.value()); }
};
