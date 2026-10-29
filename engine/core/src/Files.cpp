#include "lantern/core/Files.hpp"

#include <fstream>
#include <sstream>

namespace lantern {

std::optional<std::string> readTextFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

Status writeTextFileAtomically(const std::filesystem::path& path, std::string_view contents)
{
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return Status::failure("Could not create folder " + path.parent_path().string() + ": " + ec.message());
        }
    }

    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return Status::failure("Could not write " + temp.string());
        }
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!out) {
            return Status::failure("Could not write " + temp.string());
        }
    }

    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return Status::failure("Could not replace " + path.string());
    }
    return Status::success();
}

} // namespace lantern
