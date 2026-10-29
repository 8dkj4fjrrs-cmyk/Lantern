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

ax::DrawNode* namedShape(std::string_view name)
{
    auto* shape = ax::DrawNode::create();
    shape->setName(name);
    return shape;
}

// The lantern, centred on (0, 0) of its own node: glow, glass, flame, frame.
ax::Node* createLantern()
{
    auto* lantern = ax::Node::create();
    lantern->setName("Lantern");

    auto* glow = namedShape("Glow");
    constexpr std::array<float, 6> kGlowRadii{220.0f, 170.0f, 130.0f, 100.0f, 80.0f, 65.0f};
    for (float radius : kGlowRadii) {
        glow->drawSolidCircle(ax::Vec2::ZERO, radius, 0.0f, 64, withAlpha(kGlow, 0.07f));
    }
    glow->setContentSize(ax::Size(440.0f, 440.0f));
    lantern->addChild(glow, 0);

    auto* glass = namedShape("Glass");
    glass->drawSolidRect(ax::Vec2(-38.0f, -55.0f), ax::Vec2(38.0f, 55.0f), withAlpha(kGlow, 0.35f));
    glass->setContentSize(ax::Size(76.0f, 110.0f));
    lantern->addChild(glass, 1);

    auto* flame = namedShape("Flame");
    flame->drawSolidCircle(ax::Vec2(0.0f, -8.0f), 22.0f, 0.0f, 32, kGlow);
    flame->drawSolidCircle(ax::Vec2(0.0f, -4.0f), 12.0f, 0.0f, 32, kFlame);
    flame->setContentSize(ax::Size(44.0f, 44.0f));
    lantern->addChild(flame, 2);

    auto* frame = namedShape("Frame");
    frame->drawSolidRect(ax::Vec2(-48.0f, 55.0f), ax::Vec2(48.0f, 68.0f), kFrame);
    frame->drawSolidRect(ax::Vec2(-48.0f, -68.0f), ax::Vec2(48.0f, -55.0f), kFrame);
    frame->drawSolidRect(ax::Vec2(-42.0f, -55.0f), ax::Vec2(-36.0f, 55.0f), kFrame);
    frame->drawSolidRect(ax::Vec2(36.0f, -55.0f), ax::Vec2(42.0f, 55.0f), kFrame);
    frame->drawCircle(ax::Vec2(0.0f, 82.0f), 18.0f, 0.0f, 32, false, kFrame);
    frame->setContentSize(ax::Size(96.0f, 168.0f));
    lantern->addChild(frame, 3);

    // A slow "breathing" pulse, using Axmol's action system.
    auto* grow = ax::EaseSineInOut::create(ax::ScaleTo::create(1.2f, 1.06f));
    auto* shrink = ax::EaseSineInOut::create(ax::ScaleTo::create(1.2f, 1.0f));
    lantern->runAction(ax::RepeatForever::create(ax::Sequence::create(grow, shrink, nullptr)));

    return lantern;
}

} // namespace

ax::Node* createStartWorld(float width, float height)
{
    auto* world = ax::Node::create();
    world->setName("World");
    world->setContentSize(ax::Size(width, height));

    auto* background = namedShape("Background");
    background->drawSolidRect(ax::Vec2::ZERO, ax::Vec2(width, height), kBackground);
    background->setContentSize(ax::Size(width, height));
    world->addChild(background, -1);

    auto* lantern = createLantern();
    lantern->setPosition(width / 2.0f, height / 2.0f);
    world->addChild(lantern, 0);

    return world;
}

} // namespace lantern::detail
