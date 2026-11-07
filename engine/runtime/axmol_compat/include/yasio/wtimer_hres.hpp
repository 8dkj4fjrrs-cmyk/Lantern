// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Rolo's replacement for yasio's wtimer_hres, used by Axmol's Windows main loop.
// Written from scratch for Rolo; it contains no yasio code (see ../../README.md).
//
// Windows wakes sleeping threads on a timer that ticks every 15.6 ms by default,
// so a game that sleeps between frames would miss its frame time. While one of
// these objects exists, the timer ticks every millisecond instead.
#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <timeapi.h>
#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

namespace yasio {
struct wtimer_hres {
    wtimer_hres()
    {
        if (::timeBeginPeriod(1) == TIMERR_NOERROR)
            m_active = true;
    }
    ~wtimer_hres()
    {
        if (m_active)
            ::timeEndPeriod(1);
    }
    wtimer_hres(const wtimer_hres&) = delete;
    wtimer_hres& operator=(const wtimer_hres&) = delete;

  private:
    bool m_active = false;
};
} // namespace yasio
#endif
