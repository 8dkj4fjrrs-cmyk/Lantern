// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Rolo's replacement for yasio's set_thread_name, used by Axmol's job system.
// Written from scratch for Rolo; it contains no yasio code (see ../../README.md).
//
// Naming threads only helps debuggers and profilers show which thread is which;
// if the system cannot do it, nothing else changes.
#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <string>
#else
#include <pthread.h>
#endif

namespace yasio {
inline void set_thread_name(const char* name)
{
    if (name == nullptr)
        return;
#if defined(_WIN32)
    // SetThreadDescription exists from Windows 10 (1607). Look it up at run time
    // so the program still starts on systems without it.
    using SetDescription = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const HMODULE kernel = ::GetModuleHandleW(L"kernel32.dll");
    if (kernel == nullptr)
        return;
    const auto setDescription = reinterpret_cast<SetDescription>(
        reinterpret_cast<void*>(::GetProcAddress(kernel, "SetThreadDescription")));
    if (setDescription == nullptr)
        return;
    const int length = ::MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
    if (length <= 0)
        return;
    std::wstring wide(static_cast<size_t>(length), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, name, -1, wide.data(), length);
    setDescription(::GetCurrentThread(), wide.c_str());
#elif defined(__APPLE__)
    // macOS can only name the calling thread.
    ::pthread_setname_np(name);
#elif defined(__linux__) && defined(__GLIBC__)
    // Linux allows 15 characters plus the terminator; longer names are refused,
    // so cut them.
    char shortName[16] = {};
    for (int i = 0; i < 15 && name[i] != '\0'; ++i)
        shortName[i] = name[i];
    ::pthread_setname_np(::pthread_self(), shortName);
#endif
}
} // namespace yasio
