#include "CommandLine.hpp"

#include <charconv>
#include <limits>

namespace lantern::editor {

EditorParseResult parseEditorCommandLine(std::span<const std::string_view> args)
{
    EditorParseResult result;
    EditorOptions& opt = result.options;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];

        auto nextValue = [&](std::string_view& out) -> bool {
            if (i + 1 >= args.size()) {
                result.error = std::string(arg) + " needs a value.";
                return false;
            }
            out = args[++i];
            return true;
        };

        std::string_view value;
        if (arg == "--help" || arg == "-h") {
            opt.showHelp = true;
        } else if (arg == "--version") {
            opt.showVersion = true;
        } else if (arg == "--reset-layout") {
            opt.resetLayout = true;
        } else if (arg == "--no-project") {
            opt.noProject = true;
        } else if (arg == "--play") {
            opt.play = true;
        } else if (arg == "--project" || arg == "--new-project") {
            if (!nextValue(value)) {
                return result;
            }
            if (value.empty()) {
                result.error = std::string(arg) + " needs a folder path.";
                return result;
            }
            (arg == "--project" ? opt.projectPath : opt.newProjectPath) = std::string(value);
        } else if (arg == "--config-dir") {
            if (!nextValue(value)) {
                return result;
            }
            if (value.empty()) {
                result.error = "--config-dir needs a folder path.";
                return result;
            }
            opt.configDirectory = std::string(value);
        } else if (arg == "--frames") {
            if (!nextValue(value)) {
                return result;
            }
            std::uint64_t frames = 0;
            const char* end = value.data() + value.size();
            const auto [ptr, ec] = std::from_chars(value.data(), end, frames);
            if (ec != std::errc{} || ptr != end || frames == 0) {
                result.error = "--frames must be a whole number of at least 1, got '" + std::string(value) + "'.";
                return result;
            }
            opt.maxFrames = frames;
        } else if (arg == "--screenshot") {
            if (!nextValue(value)) {
                return result;
            }
            opt.screenshotPath = std::string(value);
        } else {
            result.error = "Unknown option: " + std::string(arg);
            return result;
        }
    }

    if (!opt.screenshotPath.empty() && opt.maxFrames == 0) {
        result.error = "--screenshot needs --frames N, so the editor knows which frame to capture.";
    }
    const int projectOptions = (opt.projectPath.empty() ? 0 : 1) + (opt.newProjectPath.empty() ? 0 : 1)
                               + (opt.noProject ? 1 : 0);
    if (projectOptions > 1) {
        result.error = "Use only one of --project, --new-project and --no-project.";
    }
    return result;
}

std::string editorHelpText()
{
    return "Usage: lantern_editor [options]\n"
           "\n"
           "Options:\n"
           "  --help              Show this message\n"
           "  --version           Print version information and exit\n"
           "  --project FOLDER    Open the Lantern project in FOLDER\n"
           "  --new-project FOLDER  Create a new project in FOLDER and open it\n"
           "  --no-project        Start without opening the last project\n"
           "  --reset-layout      Start with the default panel layout\n"
           "  --play              Start playing the scene at once (tests)\n"
           "  --config-dir DIR    Keep preferences and layout in DIR instead of the\n"
           "                      per-user settings folder\n"
           "  --frames N          Quit automatically after N frames (used by tests)\n"
           "  --screenshot FILE   Save the last frame as a PNG (needs --frames)\n";
}

} // namespace lantern::editor
