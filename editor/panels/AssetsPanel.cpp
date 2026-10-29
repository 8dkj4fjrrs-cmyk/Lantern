#include "Panels.hpp"

#include "lantern/ui/Context.hpp"

#include <algorithm>
#include <cctype>
#include <map>

namespace lantern::editor {

namespace {

constexpr double kRescanSeconds = 2.0;

bool containsIgnoringCase(std::string_view text, std::string_view needle)
{
    const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    return std::search(text.begin(), text.end(), needle.begin(), needle.end(),
                       [&](char a, char b) { return lower(a) == lower(b); })
           != text.end();
}

// Every visible file and folder in the project, project-relative, sorted so
// that each folder comes right before its contents.
std::vector<std::string> listProject(const std::filesystem::path& root)
{
    std::vector<std::string> paths;
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        const std::string leaf = it->path().filename().string();
        if (!leaf.empty() && leaf.front() == '.') {
            if (it->is_directory(ec)) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (leaf.size() > 4 && leaf.compare(leaf.size() - 4, 4, ".tmp") == 0) {
            continue; // half-written files from a save in progress
        }
        paths.push_back(it->path().lexically_relative(root).generic_string() + (it->is_directory(ec) ? "/" : ""));
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

} // namespace

AssetsPanel::AssetsPanel(EditorServices& editor)
    : ui::Column(0.0f)
    , m_editor(editor)
{
    // Without a project: an explanation and the two ways to get one.
    auto noProject = std::make_unique<ui::Column>(10.0f, ui::Insets::all(16.0f));
    noProject->setStretch(1.0f);
    noProject->setCrossAlign(ui::CrossAlign::Center);
    noProject->emplace<ui::Spacer>();
    noProject->emplace<ui::IconView>(ui::Icon::Folder, 28.0f);
    noProject->emplace<ui::Label>("No project open", ui::Label::Style::Dim);
    auto& hint = noProject->emplace<ui::Label>("A project is a folder for your game's scenes and, from Milestone 4, "
                                               "its images and sounds.",
                                               ui::Label::Style::Faint);
    hint.setWrap(true);
    hint.setAlign(ui::TextAlign::Center);
    auto& buttons = noProject->emplace<ui::Row>(8.0f);
    buttons.emplace<ui::Button>("New Project…", [this] { m_editor.newProject(); }).setStyle(ui::Button::Style::Primary);
    buttons.emplace<ui::Button>("Open…", [this] { m_editor.openProject(); });
    noProject->emplace<ui::Spacer>(1.4f);
    m_noProject = &addChild(std::move(noProject));

    // With a project: toolbar and file tree.
    auto browser = std::make_unique<ui::Column>(0.0f);
    browser->setStretch(1.0f);
    auto& toolbar = makePanelToolbar(*browser);
    ui::Button& plus = addToolButton(toolbar, ui::Icon::Plus, "Create", nullptr);
    plus.setEnabled(true);
    plus.setTooltip("Create a scene");
    plus.setOnClick([this, &plus] {
        ui::MenuItem scene;
        scene.label = "New Scene";
        scene.action = [this] { m_editor.newScene(); };
        ui::MenuItem import;
        import.label = "Import Image…";
        import.isEnabled = [] { return false; };
        openMenuAt(m_editor.ui(), plus.bounds(), {std::move(scene), ui::MenuItem::divider(), std::move(import)});
    });
    auto& search = toolbar.emplace<ui::TextField>("Search assets");
    search.setLeadingIcon(ui::Icon::Search);
    search.setClearButton(true);
    search.setStretch(1.0f);
    search.setOnChange([this](const std::string& text) {
        m_filter = text;
        rebuild();
    });
    addToolButton(toolbar, ui::Icon::Refresh, "Refresh", [this] { refresh(); });

    auto tree = std::make_unique<ui::TreeView>();
    tree->setStretch(1.0f);
    tree->setAutoExpandDepth(3);
    tree->setEmptyText("No files match.");
    tree->setOnActivate([this](std::uint64_t id) {
        if (id == 0 || id > m_paths.size() || m_editor.project() == nullptr) {
            return;
        }
        const std::string& path = m_paths[id - 1];
        if (path.size() > 7 && path.compare(path.size() - 7, 7, ".lscene") == 0) {
            m_editor.openScene(m_editor.project()->absolute(path));
        }
    });
    tree->setTooltip("Double-click a scene to open it");
    m_tree = &browser->addChild(std::move(tree));
    m_browser = &addChild(std::move(browser));
}

void AssetsPanel::refresh()
{
    m_lastScan = -100.0;
}

void AssetsPanel::update(UiFrame& frame)
{
    const Project* project = m_editor.project();
    m_noProject->setVisible(project == nullptr);
    m_browser->setVisible(project != nullptr);
    if (project == nullptr) {
        m_shownProject = nullptr;
        return;
    }
    const double now = frame.ui().time();
    if (project != m_shownProject || project->folder() != m_shownFolder || now - m_lastScan >= kRescanSeconds) {
        m_shownProject = project;
        m_shownFolder = project->folder();
        m_lastScan = now;
        m_paths = listProject(project->folder());
        rebuild();
    }
    // Highlight the open scene.
    if (const auto rel = project->relative(m_editor.document().path())) {
        const auto it = std::find(m_paths.begin(), m_paths.end(), *rel);
        if (it != m_paths.end()) {
            m_tree->setSelectedId(static_cast<std::uint64_t>(it - m_paths.begin()) + 1);
        }
    }
}

void AssetsPanel::rebuild()
{
    // A folder row for the project itself, then its files.
    std::vector<ui::TreeItem> items;
    std::map<std::string, std::uint64_t> folderIds; // "scenes/" -> item id
    for (std::size_t i = 0; i < m_paths.size(); ++i) {
        const std::string& path = m_paths[i];
        const bool folder = path.back() == '/';
        const std::string trimmed = folder ? path.substr(0, path.size() - 1) : path;
        const std::size_t slash = trimmed.rfind('/');
        const std::string name = slash == std::string::npos ? trimmed : trimmed.substr(slash + 1);
        const std::string parent = slash == std::string::npos ? "" : trimmed.substr(0, slash + 1);
        if (!m_filter.empty() && !folder && !containsIgnoringCase(name, m_filter)) {
            continue;
        }
        ui::TreeItem item;
        item.id = i + 1;
        item.label = name;
        const auto p = folderIds.find(parent);
        item.parentId = p != folderIds.end() ? p->second : 0;
        item.depth = static_cast<int>(std::count(trimmed.begin(), trimmed.end(), '/'));
        if (folder) {
            item.icon = ui::Icon::Folder;
            folderIds[path] = item.id;
        } else if (name.size() > 7 && name.compare(name.size() - 7, 7, ".lscene") == 0) {
            item.icon = ui::Icon::Flame;
            item.detail = "Scene";
        } else if (name == "project.lantern") {
            item.icon = ui::Icon::Sliders;
            item.detail = "Project settings";
        } else {
            item.dimmed = true;
        }
        items.push_back(std::move(item));
    }
    // Folders that end up empty under a search are left out.
    if (!m_filter.empty()) {
        std::vector<ui::TreeItem> kept;
        for (const ui::TreeItem& item : items) {
            const bool isFolder = m_paths[item.id - 1].back() == '/';
            const bool hasChild = std::any_of(items.begin(), items.end(), [&](const ui::TreeItem& other) {
                return other.parentId == item.id;
            });
            if (!isFolder || hasChild) {
                kept.push_back(item);
            }
        }
        items = std::move(kept);
    }
    for (ui::TreeItem& item : items) {
        item.hasChildren = std::any_of(items.begin(), items.end(),
                                       [&](const ui::TreeItem& other) { return other.parentId == item.id; });
    }
    m_tree->setItems(std::move(items));
}

} // namespace lantern::editor
