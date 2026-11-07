#include "rolo/core/Files.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace rolo {

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

std::filesystem::path executablePath()
{
    std::error_code ec;
#if defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);   // asks for the size
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return {};
    }
    buffer.resize(std::char_traits<char>::length(buffer.c_str()));
    return std::filesystem::weakly_canonical(std::filesystem::path(buffer), ec);
#elif defined(_WIN32)
    static_cast<void>(ec);
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    buffer.resize(length);
    return std::filesystem::path(buffer);
#else
    const std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", ec);
    return ec ? std::filesystem::path() : self;
#endif
}

std::optional<std::string> environmentVariable(const char* name)
{
#if defined(_WIN32)
    // _dupenv_s copies the value into memory we must free.
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) {
        return std::nullopt;
    }
    std::string value(buffer);
    std::free(buffer);
#else
    const char* raw = std::getenv(name);
    if (raw == nullptr) {
        return std::nullopt;
    }
    std::string value(raw);
#endif
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
}

} // namespace rolo
