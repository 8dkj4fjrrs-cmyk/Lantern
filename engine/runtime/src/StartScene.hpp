#pragma once

#include <axmol.h>

namespace lantern::detail {

/// The world shown when no game content is loaded yet: a softly pulsing
/// lantern on a dark background, built from a few named objects so the
/// editor's hierarchy has something real to show. It proves that drawing,
/// actions (animation) and the main loop work.
/// Replaced by real scenes from Milestone 3.
///
/// Returns an autoreleased node named "World" of the given size.
ax::Node* createStartWorld(float width, float height);

} // namespace lantern::detail
