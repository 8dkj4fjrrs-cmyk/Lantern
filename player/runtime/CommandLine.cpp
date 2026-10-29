#include "CommandLine.hpp"

#include <charconv>
#include <limits>
#include <optional>

namespace lantern::player {

namespace {

// Parses a whole string as a number within [minimum, maximum].
template <typename T>
std::optional<T> parseNumber(std::string_view text, T minimum, T maximum)
{
    T value{};
    const char* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, value);
    if (ec != std::errc{} || ptr != end || value < minimum || value > maximum) {
        return std::nullopt;
    }
    return value;
}

} // namespace

ParseResult parseCommandLine(std::span<const std::string_view> args)
{
    ParseResult result;
    PlayerOptions& opt = result.options;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];

        // Options that take a value read the next argument.
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
        } else if (arg == "--no-stats") {
            opt.showStats = false;
        } else if (arg == "--title") {
            if (!nextValue(value)) {
                return result;
            }
            opt.title = std::string(value);
        } else if (arg == "--width" || arg == "--height") {
            if (!nextValue(value)) {
                return result;
            }
            const auto number = parseNumber<int>(value, 64, 16384);
            if (!number) {
                result.error = std::string(arg) + " must be a whole number between 64 and 16384, got '"
                               + std::string(value) + "'.";
                return result;
            }
            (arg == "--width" ? opt.width : opt.height) = *number;
        } else if (arg == "--frames") {
            if (!nextValue(value)) {
                return result;
            }
            const auto number =
                parseNumber<std::uint64_t>(value, 1, std::numeric_limits<std::uint64_t>::max());
            if (!number) {
                result.error = "--frames must be a whole number of at least 1, got '" + std::string(value) + "'.";
                return result;
            }
            opt.maxFrames = *number;
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
        result.error = "--screenshot needs --frames N, so the player knows which frame to capture.";
    }
    return result;
}

std::string helpText()
{
    return "Usage: lantern_player [options]\n"
           "\n"
           "Options:\n"
           "  --help             Show this message\n"
           "  --version          Print version information and exit\n"
           "  --title TEXT       Window title (default: Lantern Engine)\n"
           "  --width N          Window width in points (default: 1280)\n"
           "  --height N         Window height in points (default: 720)\n"
           "  --no-stats         Hide the frame-rate overlay\n"
           "  --frames N         Quit automatically after N frames (used by tests)\n"
           "  --screenshot FILE  Save the last frame as a PNG (needs --frames)\n"
           "\n"
           "Press Escape or close the window to quit.\n";
}

} // namespace lantern::player
