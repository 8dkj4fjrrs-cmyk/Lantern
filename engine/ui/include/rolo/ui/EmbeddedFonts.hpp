#pragma once

// Fonts compiled into rolo_ui, so programs need no font files at run time.
// Inter (https://rsms.me/inter) by Rasmus Andersson, SIL Open Font License 1.1
// — see assets/fonts/Inter-LICENSE.txt and THIRD_PARTY_NOTICES.md.

#include <span>

namespace rolo::ui::fonts {

std::span<const unsigned char> interRegular();
std::span<const unsigned char> interSemiBold();

} // namespace rolo::ui::fonts
