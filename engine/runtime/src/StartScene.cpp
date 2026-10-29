#include "StartScene.hpp"

#include <array>

namespace lantern::detail {

namespace {

// Lantern palette.
const ax::Color4F kBackground{0.06f, 0.07f, 0.11f, 1.0f};
const ax::Color4F kGlow{1.00f, 0.72f, 0.30f, 1.0f};
const ax::Color4F kFlame{1.00f, 0.88f, 0.55f, 1.0f};
const ax::Color4F kFrame{0.22f, 0.20f, 0.24f, 1.0f};

ax::Color4F withAlpha(ax::Color4F color, float alpha)
{
    color.a = alpha;
    return color;
}

// Draws a simple lantern centred on (0, 0): glow, glass, flame, frame.
ax::DrawNode* createLanternDrawing()
{
    auto* draw = ax::DrawNode::create();

    // Soft glow: wide, faint circles stacked so the centre is brightest.
    constexpr std::array<float, 6> kGlowRadii{220.0f, 170.0f, 130.0f, 100.0f, 80.0f, 65.0f};
    for (float radius : kGlowRadii) {
        draw->drawSolidCircle(ax::Vec2::ZERO, radius, 0.0f, 64, withAlpha(kGlow, 0.07f));
    }

    // Glass body and flame.
    draw->drawSolidRect(ax::Vec2(-38.0f, -55.0f), ax::Vec2(38.0f, 55.0f), withAlpha(kGlow, 0.35f));
    draw->drawSolidCircle(ax::Vec2(0.0f, -8.0f), 22.0f, 0.0f, 32, kGlow);
    draw->drawSolidCircle(ax::Vec2(0.0f, -4.0f), 12.0f, 0.0f, 32, kFlame);

    // Metal frame: top cap, base, handle.
    draw->drawSolidRect(ax::Vec2(-48.0f, 55.0f), ax::Vec2(48.0f, 68.0f), kFrame);
    draw->drawSolidRect(ax::Vec2(-48.0f, -68.0f), ax::Vec2(48.0f, -55.0f), kFrame);
    draw->drawSolidRect(ax::Vec2(-42.0f, -55.0f), ax::Vec2(-36.0f, 55.0f), kFrame);
    draw->drawSolidRect(ax::Vec2(36.0f, -55.0f), ax::Vec2(42.0f, 55.0f), kFrame);
    draw->drawCircle(ax::Vec2(0.0f, 82.0f), 18.0f, 0.0f, 32, false, kFrame);

    return draw;
}

} // namespace

ax::Scene* createStartScene()
{
    auto* scene = ax::Scene::create();
    ax::Director::getInstance()->setClearColor(kBackground);

    const ax::Vec2 origin = ax::Director::getInstance()->getVisibleOrigin();
    const ax::Vec2 size = ax::Director::getInstance()->getVisibleSize();

    auto* lantern = createLanternDrawing();
    lantern->setPosition(origin + size / 2.0f);
    scene->addChild(lantern);

    // A slow "breathing" pulse, using Axmol's action system.
    auto* grow = ax::EaseSineInOut::create(ax::ScaleTo::create(1.2f, 1.06f));
    auto* shrink = ax::EaseSineInOut::create(ax::ScaleTo::create(1.2f, 1.0f));
    lantern->runAction(ax::RepeatForever::create(ax::Sequence::create(grow, shrink, nullptr)));

    return scene;
}

} // namespace lantern::detail
