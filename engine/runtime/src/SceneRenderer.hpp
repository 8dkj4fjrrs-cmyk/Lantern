#pragma once

// SceneRenderer: shows a lantern::Scene with Axmol nodes.
//
// Lantern scenes are plain data; Axmol draws node trees. The renderer keeps
// an Axmol node tree that mirrors the scene: one node per entity, with a
// DrawNode for entities that have a Shape. When the scene changes (its
// revision number goes up) the mirror is rebuilt. Rebuilding everything is
// simple and fast enough for editor-sized scenes; finer-grained updates can
// come later without changing this interface.
//
// Internal to lantern_runtime.

#include "lantern/scene/Scene.hpp"

#include <axmol.h>

#include <memory>

namespace lantern::detail {

class SceneRenderer {
public:
    /// `root` is the node the scene is drawn under (owned by the Axmol scene).
    /// With `drawShapes` false, shapes get plain nodes instead of DrawNodes:
    /// DrawNodes need a graphics context, which unit tests do not have.
    explicit SceneRenderer(ax::Node& root, bool drawShapes = true);
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    /// Shows `scene` (nothing for null). Takes effect at the next update().
    void show(std::shared_ptr<const Scene> scene);
    const std::shared_ptr<const Scene>& scene() const noexcept { return m_scene; }

    /// Rebuilds the Axmol nodes if the scene or its contents changed.
    void update();

    /// Number of times the node tree has been built (for diagnostics and tests).
    std::uint64_t buildCount() const noexcept { return m_builds; }

private:
    void rebuild();

    ax::Node& m_root;
    bool m_drawShapes;
    ax::Node* m_container = nullptr;   // child of m_root; holds the mirror
    std::shared_ptr<const Scene> m_scene;
    const Scene* m_builtScene = nullptr;
    std::uint64_t m_builtRevision = 0;
    std::uint64_t m_builds = 0;
};

} // namespace lantern::detail
