#pragma once

// DockLayout: the arrangement of docked panels, as plain data.
//
// The layout is a binary tree. Each branch ("split") divides its area in two,
// side by side or one above the other, at a ratio the user can drag. Each
// leaf is a group of tabs, one of them active. Panels are named by short ids
// ("viewport", "console").
//
// All operations keep the tree tidy: a tab group that becomes empty disappears
// and its neighbour takes its space.
//
// Saved as one line of text, e.g.
//     rolo-dock 1 (h 0.18 [0|hierarchy] (h 0.76 (v 0.7 [0|viewport] [0|console]) [0|inspector]))

#include "rolo/ui/Geometry.hpp"
#include "rolo/ui/widgets/Box.hpp" // Axis

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rolo::ui {

enum class DockSide { Left, Right, Top, Bottom, Center };

class DockLayout {
public:
    struct Node {
        // Branch: two children split along `axis` (Horizontal = side by side).
        bool isSplit = false;
        Axis axis = Axis::Horizontal;
        float ratio = 0.5f;  // share of the first child
        std::unique_ptr<Node> first;
        std::unique_ptr<Node> second;
        // Leaf: a group of tabs.
        std::vector<std::string> tabs;
        std::size_t active = 0;

        Node* parent = nullptr;
    };

    static constexpr int kFormatVersion = 1;

    DockLayout();
    DockLayout(const DockLayout& other);
    DockLayout& operator=(const DockLayout& other);
    DockLayout(DockLayout&&) noexcept = default;
    DockLayout& operator=(DockLayout&&) noexcept = default;

    /// A layout with one tab group holding `panels`.
    static DockLayout tabs(std::vector<std::string> panels);

    const Node& root() const noexcept { return *m_root; }
    bool isEmpty() const noexcept;
    bool contains(std::string_view panel) const;
    /// Every docked panel, in tree order.
    std::vector<std::string> panels() const;
    /// The tab group holding `panel`, if docked.
    const Node* groupOf(std::string_view panel) const;
    /// Is `panel` the visible tab of its group?
    bool isActive(std::string_view panel) const;

    // --- Changes -------------------------------------------------------------------
    /// Moves (or adds) `panel` next to the group containing `anchor`:
    /// as a new tab (Center) or in a new group on one side, taking `share` of the space.
    /// Returns false if impossible (e.g. anchor not docked, or docking a lone panel onto itself).
    bool dock(const std::string& panel, std::string_view anchor, DockSide side, float share = 0.3f);
    /// Adds `panel` as the first group if the layout is empty.
    void addFirst(const std::string& panel);
    /// Removes `panel`; an emptied group collapses.
    bool remove(std::string_view panel);
    /// Makes `panel` the visible tab of its group.
    void activate(std::string_view panel);
    /// Sets a split's ratio (clamped to 5%..95%).
    static void setRatio(Node& split, float ratio);

    /// Puts a closed panel back roughly where it is in `reference` (usually the
    /// default layout): beside a neighbour from its old place, or as a tab of
    /// the largest group if none is open.
    void reopen(const std::string& panel, const DockLayout& reference);

    // --- Geometry ---------------------------------------------------------------------
    struct GroupRect {
        const Node* node;
        Rect rect;
    };
    struct SplitterRect {
        Node* node;  // the split it resizes
        Rect rect;   // the draggable gap
        Rect area;   // the split's whole area (for converting drags to ratios)
    };
    /// Divides `area` among the groups, leaving `gap` points between neighbours.
    void computeRects(Rect area, float gap, std::vector<GroupRect>& groups, std::vector<SplitterRect>& splitters);

    // --- Saving -----------------------------------------------------------------------------
    std::string serialize() const;
    /// Reads serialize() output. Panels for which `isKnownPanel` is false are
    /// dropped (e.g. from a newer editor). Returns nothing if unreadable or empty.
    static std::optional<DockLayout> parse(std::string_view text,
                                           const std::function<bool(std::string_view)>& isKnownPanel);

private:
    Node* findGroup(std::string_view panel);
    void collapse(Node* emptyLeaf);
    /// Turns `target` (a group or a whole branch) into a split of [new group, target].
    void splitBeside(Node* target, const std::string& panel, DockSide side, float share);
    /// The smallest branch containing every one of `panels` (they must all be docked).
    Node* commonAncestor(const std::vector<std::string>& panels);

    std::unique_ptr<Node> m_root;
};

} // namespace rolo::ui
