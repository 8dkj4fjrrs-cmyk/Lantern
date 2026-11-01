#include "lantern/gameplay/Input.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace lantern;

TEST_CASE("Keys are down, pressed this frame, released this frame", "[gameplay][input]")
{
    Input input;
    CHECK_FALSE(input.isDown(Key::Space));
    input.press(Key::Space);
    CHECK(input.isDown(Key::Space));
    CHECK(input.wasPressed(Key::Space));
    input.press(Key::Space);   // key repeat: still one press
    input.endFrame();
    CHECK(input.isDown(Key::Space));
    CHECK_FALSE(input.wasPressed(Key::Space));
    input.release(Key::Space);
    CHECK(input.wasReleased(Key::Space));
    CHECK_FALSE(input.isDown(Key::Space));
    input.endFrame();
    CHECK_FALSE(input.wasReleased(Key::Space));
}

TEST_CASE("A tap shorter than a frame still counts as a press", "[gameplay][input]")
{
    Input input;
    input.press(Key::Enter);
    input.release(Key::Enter);
    CHECK(input.wasPressed(Key::Enter));
    CHECK(input.wasReleased(Key::Enter));
    CHECK_FALSE(input.isDown(Key::Enter));
}

TEST_CASE("Move direction from arrows or WASD, never faster diagonally", "[gameplay][input]")
{
    Input input;
    CHECK(input.moveDirection() == Vec2{0, 0});
    input.press(Key::Right);
    CHECK(input.moveDirection() == Vec2{1, 0});
    input.press(Key::W);
    const Vec2 d = input.moveDirection();
    CHECK(d.x == Catch::Approx(0.7071f));
    CHECK(d.y == Catch::Approx(0.7071f));
    input.press(Key::A);   // left and right cancel out
    CHECK(input.moveDirection().x == 0.0f);
    CHECK(input.moveDirection().y == 1.0f);
}

TEST_CASE("Releasing everything lets go of held keys and buttons", "[gameplay][input]")
{
    Input input;
    input.press(Key::Left);
    input.press(MouseButton::Left);
    input.setPointer({3, 4});
    input.endFrame();
    input.releaseAll();
    CHECK_FALSE(input.isDown(Key::Left));
    CHECK(input.wasReleased(Key::Left));
    CHECK_FALSE(input.isDown(MouseButton::Left));
    CHECK(input.pointer() == Vec2{3, 4});
    CHECK(keyName(Key::Left) == "Left");
    CHECK(keyName(Key::Num7) == "7");
    CHECK(keyName(Key::Count) == "Unknown");
}
