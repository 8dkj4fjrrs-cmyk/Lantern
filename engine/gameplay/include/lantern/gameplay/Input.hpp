#pragma once

// Input: the keyboard and mouse as gameplay code sees them.
//
// Gameplay asks questions about the current frame:
//
//     if (game.input().isDown(Key::Right)) { ... }        // held down now
//     if (game.input().wasPressed(Key::Space)) { jump(); } // went down this frame
//     Vec2 direction = game.input().moveDirection();      // arrows or WASD
//
// The program running the game (the player, or the editor's play mode)
// reports key and button changes with press()/release() as they happen, and
// calls endFrame() after each game update, which clears the "this frame"
// flags. Lantern's Key names do not depend on the window library, so game
// code never sees GLFW or Axmol key codes.

#include "lantern/scene/Types.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace lantern {

enum class Key {
    Unknown,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Space, Enter, Escape, Tab, Backspace,
    Left, Right, Up, Down,
    LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Count   // not a key: the number of keys
};

/// The key's name, e.g. "Space", "A", "Left".
std::string_view keyName(Key key);

enum class MouseButton { Left, Right, Middle, Count };

class Input {
public:
    // --- Asked by gameplay ---------------------------------------------------------------
    /// The key is held down.
    bool isDown(Key key) const noexcept { return state(key).down; }
    /// The key went down since the last frame.
    bool wasPressed(Key key) const noexcept { return state(key).pressed; }
    /// The key went up since the last frame.
    bool wasReleased(Key key) const noexcept { return state(key).released; }

    bool isDown(MouseButton button) const noexcept { return state(button).down; }
    bool wasPressed(MouseButton button) const noexcept { return state(button).pressed; }
    bool wasReleased(MouseButton button) const noexcept { return state(button).released; }
    /// Where the mouse pointer is, in world coordinates (as last reported).
    Vec2 pointer() const noexcept { return m_pointer; }

    /// The direction the arrow keys or W A S D point in, each axis -1, 0 or 1
    /// and the whole at most 1 long (moving diagonally is not faster).
    Vec2 moveDirection() const noexcept;

    // --- Reported by the program running the game -------------------------------------------
    void press(Key key) noexcept;
    void release(Key key) noexcept;
    void press(MouseButton button) noexcept;
    void release(MouseButton button) noexcept;
    void setPointer(Vec2 worldPosition) noexcept { m_pointer = worldPosition; }
    /// Lets go of everything, e.g. when the window or game view loses focus
    /// (otherwise a key released elsewhere would stay "down").
    void releaseAll() noexcept;
    /// Clears the "pressed/released this frame" flags. Call after each update.
    void endFrame() noexcept;

private:
    struct State {
        bool down = false;
        bool pressed = false;
        bool released = false;
    };
    const State& state(Key key) const noexcept { return m_keys[index(key)]; }
    const State& state(MouseButton b) const noexcept { return m_buttons[static_cast<std::size_t>(b)]; }
    static std::size_t index(Key key) noexcept
    {
        const auto i = static_cast<std::size_t>(key);
        return i < static_cast<std::size_t>(Key::Count) ? i : 0;
    }
    static void down(State& s) noexcept;
    static void up(State& s) noexcept;

    std::array<State, static_cast<std::size_t>(Key::Count)> m_keys{};
    std::array<State, static_cast<std::size_t>(MouseButton::Count)> m_buttons{};
    Vec2 m_pointer;
};

} // namespace lantern
