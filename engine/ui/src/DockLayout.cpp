#include "lantern/ui/DockLayout.hpp"

#include "lantern/core/Numbers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lantern::ui {

namespace {

using Node = DockLayout::Node;

std::unique_ptr<Node> cloneNode(const Node& src, Node* parent)
{
    auto n = std::make_unique<Node>();
    n->isSplit = src.isSplit;
    n->axis = src.axis;
    n->ratio = src.ratio;
    n->tabs = src.tabs;
    n->active = src.active;
    n->parent = parent;
    if (src.isSplit) {
        n->first = cloneNode(*src.first, n.get());
        n->second = cloneNode(*src.second, n.get());
    }
    return n;
}

void collectPanels(const Node& n, std::vector<std::string>& out)
{
    if (n.isSplit) {
        collectPanels(*n.first, out);
        collectPanels(*n.second, out);
    } else {
        out.insert(out.end(), n.tabs.begin(), n.tabs.end());
    }
}

const Node* findIn(const Node& n, std::string_view panel)
{
    if (n.isSplit) {
        if (const Node* f = findIn(*n.first, panel)) {
            return f;
        }
        return findIn(*n.second, panel);
    }
    return std::find(n.tabs.begin(), n.tabs.end(), panel) != n.tabs.end() ? &n : nullptr;
}

// Moves the contents of `from` into `to` (keeping `to`'s parent link).
void moveInto(Node& to, Node&& from)
{
    Node* parent = to.parent;
    to.isSplit = from.isSplit;
    to.axis = from.axis;
    to.ratio = from.ratio;
    to.tabs = std::move(from.tabs);
    to.active = from.active;
    to.first = std::move(from.first);
    to.second = std::move(from.second);
    to.parent = parent;
    if (to.first) {
        to.first->parent = &to;
    }
    if (to.second) {
        to.second->parent = &to;
    }
}

float area(const Rect& r)
{
    return r.width * r.height;
}

} // namespace

DockLayout::DockLayout()
    : m_root(std::make_unique<Node>())
{
}

DockLayout::DockLayout(const DockLayout& other)
    : m_root(cloneNode(*other.m_root, nullptr))
{
}

DockLayout& DockLayout::operator=(const DockLayout& other)
{
    if (this != &other) {
        m_root = cloneNode(*other.m_root, nullptr);
    }
    return *this;
}

DockLayout DockLayout::tabs(std::vector<std::string> panels)
{
    DockLayout layout;
    layout.m_root->tabs = std::move(panels);
    return layout;
}

bool DockLayout::isEmpty() const noexcept
{
    return !m_root->isSplit && m_root->tabs.empty();
}

bool DockLayout::contains(std::string_view panel) const
{
    return findIn(*m_root, panel) != nullptr;
}

std::vector<std::string> DockLayout::panels() const
{
    std::vector<std::string> out;
    collectPanels(*m_root, out);
    return out;
}

const Node* DockLayout::groupOf(std::string_view panel) const
{
    return findIn(*m_root, panel);
}

Node* DockLayout::findGroup(std::string_view panel)
{
    return const_cast<Node*>(findIn(*m_root, panel));
}

bool DockLayout::isActive(std::string_view panel) const
{
    const Node* g = groupOf(panel);
    return g != nullptr && g->active < g->tabs.size() && g->tabs[g->active] == panel;
}

void DockLayout::activate(std::string_view panel)
{
    if (Node* g = findGroup(panel)) {
        g->active = static_cast<std::size_t>(std::find(g->tabs.begin(), g->tabs.end(), panel) - g->tabs.begin());
    }
}

void DockLayout::setRatio(Node& split, float ratio)
{
    split.ratio = std::clamp(ratio, 0.05f, 0.95f);
}

void DockLayout::collapse(Node* leaf)
{
    if (leaf->isSplit || !leaf->tabs.empty() || leaf->parent == nullptr) {
        return;
    }
    Node* parent = leaf->parent;
    // Take ownership of the sibling first, then free the empty leaf, then let
    // the sibling's contents take the parent's place.
    std::unique_ptr<Node> keep = parent->first.get() == leaf ? std::move(parent->second) : std::move(parent->first);
    parent->first.reset();
    parent->second.reset(); // destroys `leaf`
    moveInto(*parent, std::move(*keep));
}

bool DockLayout::remove(std::string_view panel)
{
    Node* g = findGroup(panel);
    if (g == nullptr) {
        return false;
    }
    const auto index = static_cast<std::size_t>(std::find(g->tabs.begin(), g->tabs.end(), panel) - g->tabs.begin());
    g->tabs.erase(g->tabs.begin() + static_cast<std::ptrdiff_t>(index));
    if (g->active > index || g->active >= g->tabs.size()) {
        g->active = g->active > 0 ? g->active - 1 : 0;
    }
    collapse(g);
    return true;
}

void DockLayout::addFirst(const std::string& panel)
{
    if (isEmpty()) {
        m_root->tabs = {panel};
        m_root->active = 0;
    }
}

bool DockLayout::dock(const std::string& panel, std::string_view anchor, DockSide side, float share)
{
    if (!contains(anchor)) {
        return false;
    }
    if (panel == anchor) {
        return false;
    }
    // Docking onto its own group: only meaningful if the group has other tabs.
    const Node* own = groupOf(panel);
    if (own != nullptr && own == groupOf(anchor) && side == DockSide::Center) {
        activate(panel);
        return true;
    }

    remove(panel);               // may reshape the tree...
    Node* target = findGroup(anchor); // ...so find the anchor again afterwards
    if (target == nullptr) {
        return false;
    }

    if (side == DockSide::Center) {
        target->tabs.push_back(panel);
        target->active = target->tabs.size() - 1;
        return true;
    }

    splitBeside(target, panel, side, share);
    return true;
}

void DockLayout::splitBeside(Node* target, const std::string& panel, DockSide side, float share)
{
    // Move the target's whole contents (a group or a branch) down one level...
    auto oldPart = std::make_unique<Node>();
    moveInto(*oldPart, std::move(*target));
    oldPart->parent = nullptr;
    auto newGroup = std::make_unique<Node>();
    newGroup->tabs = {panel};

    // ...and make the target a split holding [new group | old contents].
    const bool newFirst = side == DockSide::Left || side == DockSide::Top;
    target->isSplit = true;
    target->tabs.clear();
    target->active = 0;
    target->axis = (side == DockSide::Left || side == DockSide::Right) ? Axis::Horizontal : Axis::Vertical;
    const float s = std::clamp(share, 0.1f, 0.9f);
    target->ratio = newFirst ? s : 1.0f - s;
    target->first = newFirst ? std::move(newGroup) : std::move(oldPart);
    target->second = newFirst ? std::move(oldPart) : std::move(newGroup);
    target->first->parent = target;
    target->second->parent = target;
}

DockLayout::Node* DockLayout::commonAncestor(const std::vector<std::string>& panels)
{
    Node* result = nullptr;
    for (const std::string& panel : panels) {
        Node* group = findGroup(panel);
        if (group == nullptr) {
            continue;
        }
        if (result == nullptr) {
            result = group;
            continue;
        }
        // Walk up from `result` until it contains `group`.
        auto contains = [](const Node* ancestor, const Node* n) {
            for (; n != nullptr; n = n->parent) {
                if (n == ancestor) {
                    return true;
                }
            }
            return false;
        };
        while (result != nullptr && !contains(result, group)) {
            result = result->parent;
        }
    }
    return result;
}

void DockLayout::reopen(const std::string& panel, const DockLayout& reference)
{
    if (contains(panel)) {
        activate(panel);
        return;
    }
    if (isEmpty()) {
        addFirst(panel);
        return;
    }
    // Where was it in the reference layout, and who was its neighbour?
    if (const Node* refGroup = reference.groupOf(panel)) {
        // Prefer joining an open tab-mate from the same reference group.
        for (const std::string& mate : refGroup->tabs) {
            if (mate != panel && contains(mate)) {
                dock(panel, mate, DockSide::Center);
                return;
            }
        }
        if (const Node* split = refGroup->parent) {
            // Its old neighbourhood: everything on the other side of its split.
            const bool wasFirst = split->first.get() == refGroup;
            std::vector<std::string> neighbours;
            collectPanels(wasFirst ? *split->second : *split->first, neighbours);
            std::vector<std::string> open;
            for (const std::string& n : neighbours) {
                if (contains(n)) {
                    open.push_back(n);
                }
            }
            if (Node* target = commonAncestor(open)) {
                const DockSide side = split->axis == Axis::Horizontal
                                          ? (wasFirst ? DockSide::Left : DockSide::Right)
                                          : (wasFirst ? DockSide::Top : DockSide::Bottom);
                const float share = wasFirst ? split->ratio : 1.0f - split->ratio;
                splitBeside(target, panel, side, share);
                return;
            }
        }
    }
    // Fallback: a tab in the biggest group.
    std::vector<GroupRect> groups;
    std::vector<SplitterRect> splitters;
    computeRects(Rect{0, 0, 1000, 1000}, 0.0f, groups, splitters);
    const auto biggest = std::max_element(groups.begin(), groups.end(),
                                          [](const GroupRect& a, const GroupRect& b) { return area(a.rect) < area(b.rect); });
    if (biggest != groups.end() && !biggest->node->tabs.empty()) {
        dock(panel, biggest->node->tabs.front(), DockSide::Center);
    }
}

void DockLayout::computeRects(Rect rect, float gap, std::vector<GroupRect>& groups,
                              std::vector<SplitterRect>& splitters)
{
    groups.clear();
    splitters.clear();
    std::function<void(Node&, Rect)> visit = [&](Node& n, Rect r) {
        if (!n.isSplit) {
            groups.push_back({&n, r});
            return;
        }
        const bool horizontal = n.axis == Axis::Horizontal;
        const float total = (horizontal ? r.width : r.height) - gap;
        // Keep both sides usable (at least ~80 points) when there is room.
        const float minSide = std::min(80.0f, total * 0.5f);
        const float firstSize = std::round(std::clamp(total * n.ratio, minSide, std::max(minSide, total - minSide)));
        Rect a = r;
        Rect b = r;
        Rect g = r;
        if (horizontal) {
            a.width = firstSize;
            g = Rect{r.x + firstSize, r.y, gap, r.height};
            b = Rect{r.x + firstSize + gap, r.y, r.width - firstSize - gap, r.height};
        } else {
            a.height = firstSize;
            g = Rect{r.x, r.y + firstSize, r.width, gap};
            b = Rect{r.x, r.y + firstSize + gap, r.width, r.height - firstSize - gap};
        }
        splitters.push_back({&n, g, r});
        visit(*n.first, a);
        visit(*n.second, b);
    };
    visit(*m_root, rect);
}

// --- Saving ----------------------------------------------------------------------------------

std::string DockLayout::serialize() const
{
    std::string out = "lantern-dock " + std::to_string(kFormatVersion) + " ";
    std::function<void(const Node&)> write = [&](const Node& n) {
        if (n.isSplit) {
            char ratio[16];
            std::snprintf(ratio, sizeof ratio, "%.3f", static_cast<double>(n.ratio));
            out += n.axis == Axis::Horizontal ? "(h " : "(v ";
            out += ratio;
            out += ' ';
            write(*n.first);
            out += ' ';
            write(*n.second);
            out += ')';
        } else {
            out += '[' + std::to_string(n.active) + '|';
            for (std::size_t i = 0; i < n.tabs.size(); ++i) {
                out += (i > 0 ? "," : "") + n.tabs[i];
            }
            out += ']';
        }
    };
    write(*m_root);
    return out;
}

namespace {

// A small recursive-descent reader for the format written above.
struct Reader {
    std::string_view text;
    std::size_t pos = 0;
    const std::function<bool(std::string_view)>& known;

    void skipSpaces()
    {
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\r' || text[pos] == '\t')) {
            ++pos;
        }
    }
    bool expect(char c)
    {
        skipSpaces();
        if (pos < text.size() && text[pos] == c) {
            ++pos;
            return true;
        }
        return false;
    }
    std::optional<float> number()
    {
        skipSpaces();
        const std::size_t start = pos;
        while (pos < text.size() && ((text[pos] >= '0' && text[pos] <= '9') || text[pos] == '.')) {
            ++pos;
        }
        if (start == pos) {
            return std::nullopt;
        }
        const auto value = parseNumber(text.substr(start, pos - start));
        if (!value) {
            return std::nullopt;
        }
        return static_cast<float>(*value);
    }
    static bool isIdChar(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    }

    // Returns null for unreadable input; an empty leaf if every panel was unknown.
    std::unique_ptr<Node> node(int depth)
    {
        if (depth > 32) {
            return nullptr;
        }
        skipSpaces();
        if (expect('(')) {
            skipSpaces();
            if (pos >= text.size() || (text[pos] != 'h' && text[pos] != 'v')) {
                return nullptr;
            }
            auto n = std::make_unique<Node>();
            n->isSplit = true;
            n->axis = text[pos] == 'h' ? Axis::Horizontal : Axis::Vertical;
            ++pos;
            const auto ratio = number();
            if (!ratio) {
                return nullptr;
            }
            DockLayout::setRatio(*n, *ratio);
            n->first = node(depth + 1);
            n->second = node(depth + 1);
            if (!n->first || !n->second || !expect(')')) {
                return nullptr;
            }
            n->first->parent = n.get();
            n->second->parent = n.get();
            return n;
        }
        if (expect('[')) {
            auto n = std::make_unique<Node>();
            const auto active = number();
            if (!active || !expect('|')) {
                return nullptr;
            }
            std::size_t activeIndex = static_cast<std::size_t>(*active);
            std::size_t index = 0;
            while (true) {
                skipSpaces();
                const std::size_t start = pos;
                while (pos < text.size() && isIdChar(text[pos])) {
                    ++pos;
                }
                const std::string_view id = text.substr(start, pos - start);
                if (!id.empty()) {
                    if (known(id) && std::find(n->tabs.begin(), n->tabs.end(), id) == n->tabs.end()) {
                        n->tabs.emplace_back(id);
                    } else if (index < activeIndex && activeIndex > 0) {
                        --activeIndex; // keep pointing at the same tab
                    }
                    ++index;
                }
                if (expect(',')) {
                    continue;
                }
                if (expect(']')) {
                    break;
                }
                return nullptr;
            }
            n->active = n->tabs.empty() ? 0 : std::min(activeIndex, n->tabs.size() - 1);
            return n;
        }
        return nullptr;
    }
};

// Removes empty groups left by unknown panels, collapsing their splits.
std::unique_ptr<Node> prune(std::unique_ptr<Node> n)
{
    if (!n->isSplit) {
        return n->tabs.empty() ? nullptr : std::move(n);
    }
    auto a = prune(std::move(n->first));
    auto b = prune(std::move(n->second));
    if (!a && !b) {
        return nullptr;
    }
    if (!a || !b) {
        auto& only = a ? a : b;
        only->parent = nullptr;
        return std::move(only);
    }
    n->first = std::move(a);
    n->second = std::move(b);
    n->first->parent = n.get();
    n->second->parent = n.get();
    return n;
}

} // namespace

std::optional<DockLayout> DockLayout::parse(std::string_view text, const std::function<bool(std::string_view)>& isKnownPanel)
{
    constexpr std::string_view kMagic = "lantern-dock";
    while (!text.empty() && (text.front() == ' ' || text.front() == '\n')) {
        text.remove_prefix(1);
    }
    if (text.substr(0, kMagic.size()) != kMagic) {
        return std::nullopt;
    }
    Reader reader{text, kMagic.size(), isKnownPanel};
    const auto version = reader.number();
    if (!version || static_cast<int>(*version) != kFormatVersion) {
        return std::nullopt; // a newer format: fall back to the default layout
    }
    auto root = reader.node(0);
    if (!root) {
        return std::nullopt;
    }
    root = prune(std::move(root));
    if (!root) {
        return std::nullopt;
    }
    // Re-link parents (prune may have moved nodes).
    std::function<void(Node&, Node*)> relink = [&](Node& n, Node* parent) {
        n.parent = parent;
        if (n.isSplit) {
            relink(*n.first, &n);
            relink(*n.second, &n);
        }
    };
    relink(*root, nullptr);
    DockLayout layout;
    layout.m_root = std::move(root);
    return layout;
}

} // namespace lantern::ui
