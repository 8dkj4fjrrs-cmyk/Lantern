#pragma once

#include <axmol.h>

namespace lantern::detail {

/// The scene shown when no game content is loaded yet: a softly pulsing
/// lantern glow on a dark background. It exists to prove that windowing,
/// drawing, actions (animation) and the main loop all work.
/// Replaced by real scenes from Milestone 3.
ax::Scene* createStartScene();

} // namespace lantern::detail
