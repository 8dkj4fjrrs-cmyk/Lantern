#include "lantern/gameplay/Input.hpp"

#include <cmath>

namespace lantern {

std::string_view keyName(Key key)
{
    static constexpr std::string_view kNames[] = {
        "Unknown",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V",
        "W", "X", "Y", "Z",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "Space", "Enter", "Escape", "Tab", "Backspace",
        "Left", "Right", "Up", "Down",
        "Left Shift", "Right Shift", "Left Control", "Right Control", "Left Alt", "Right Alt",
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    };
    static_assert(std::size(kNames) == static_cast<std::size_t>(Key::Count), "a name for every key");
    const auto i = static_cast<std::size_t>(key);
    return i < std::size(kNames) ? kNames[i] : kNames[0];
}

Vec2 Input::moveDirection() const noexcept
{
    float x = 0.0f;
    float y = 0.0f;
    if (isDown(Key::Left) || isDown(Key::A)) {
        x -= 1.0f;
    }
    if (isDown(Key::Right) || isDown(Key::D)) {
        x += 1.0f;
    }
    if (isDown(Key::Down) || isDown(Key::S)) {
        y -= 1.0f;
    }
    if (isDown(Key::Up) || isDown(Key::W)) {
        y += 1.0f;
    }
    if (x != 0.0f && y != 0.0f) {
        // Diagonal: scale to length 1, or it would be ~1.41 times as fast.
        const float s = 1.0f / std::sqrt(2.0f);
        x *= s;
        y *= s;
    }
    return {x, y};
}

void Input::down(State& s) noexcept
{
    if (!s.down) {
        s.down = true;
        s.pressed = true;
    }
}

void Input::up(State& s) noexcept
{
    if (s.down) {
        s.down = false;
        s.released = true;
    }
}

void Input::press(Key key) noexcept
{
    if (key != Key::Unknown) {
        down(m_keys[index(key)]);
    }
}

void Input::release(Key key) noexcept
{
    if (key != Key::Unknown) {
        up(m_keys[index(key)]);
    }
}

void Input::press(MouseButton button) noexcept
{
    if (button != MouseButton::Count) {
        down(m_buttons[static_cast<std::size_t>(button)]);
    }
}

void Input::release(MouseButton button) noexcept
{
    if (button != MouseButton::Count) {
        up(m_buttons[static_cast<std::size_t>(button)]);
    }
}

void Input::releaseAll() noexcept
{
    for (State& s : m_keys) {
        up(s);
    }
    for (State& s : m_buttons) {
        up(s);
    }
}

void Input::endFrame() noexcept
{
    for (State& s : m_keys) {
        s.pressed = false;
        s.released = false;
    }
    for (State& s : m_buttons) {
        s.pressed = false;
        s.released = false;
    }
}

} // namespace lantern
