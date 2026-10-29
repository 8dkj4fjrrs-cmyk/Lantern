#pragma once

// Fonts compiled into lantern_ui, so programs need no font files at run time.
// Inter (https://rsms.me/inter) by Rasmus Andersson, SIL Open Font License 1.1
// — see assets/fonts/Inter-LICENSE.txt and THIRD_PARTY_NOTICES.md.

#include <span>

namespace lantern::ui::fonts {

std::span<const unsigned char> interRegular();
std::span<const unsigned char> interSemiBold();

} // namespace lantern::ui::fonts
