#pragma once

// Builds the read-only SceneNodeInfo snapshot used by tools (the editor's
// hierarchy and inspector). Internal to lantern_runtime.

#include "lantern/runtime/Application.hpp"

#include <axmol.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace lantern::detail {

/// Gives each backend node a small number that stays the same for as long as
/// the node exists, so a selection survives from one frame to the next.
/// (Session-only: Lantern's saved scenes get their own stable IDs in Milestone 3.)
class NodeIdRegistry {
public:
    /// Returns the node's id, assigning the next free one on first sight.
    std::uint64_t idFor(const ax::Node* node);

    /// Forgets every node not in `alive` (call after a full traversal), so a
    /// new node that reuses a freed address gets a fresh id.
    void retainOnly(const std::vector<const ax::Node*>& alive);

    std::size_t size() const noexcept { return m_ids.size(); }

private:
    std::unordered_map<const ax::Node*, std::uint64_t> m_ids;
    std::uint64_t m_nextId = 1;
};

/// Short, user-facing kind name for a node ("Shape", "Sprite", ...).
std::string nodeKind(const ax::Node& node);

/// Appends `root` and all its descendants (depth-first, parents first,
/// children in draw order) to `out`.
void buildSceneOutline(const ax::Node& root, NodeIdRegistry& ids, std::vector<SceneNodeInfo>& out);

} // namespace lantern::detail
