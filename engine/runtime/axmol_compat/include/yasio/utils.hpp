// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Rolo's replacement for the clock helpers Axmol's core takes from yasio.
// Written from scratch for Rolo; it contains no yasio code (see ../../README.md).
#pragma once

#include <algorithm>
#include <chrono>
#include <ctime>

namespace yasio {
using highp_time_t = long long;
using steady_clock_t = std::chrono::steady_clock;
using system_clock_t = std::chrono::system_clock;

// Time since the clock's epoch, in nanoseconds / microseconds / milliseconds.
// The steady clock (the default) never jumps, so it is the one to measure
// durations with; system_clock_t gives wall-clock time (used for log stamps).
template <typename Clock = steady_clock_t> inline highp_time_t xhighp_clock()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}

template <typename Clock = steady_clock_t> inline highp_time_t highp_clock()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count();
}

template <typename Clock = steady_clock_t> inline highp_time_t clock()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

// Seconds since 1970.
inline highp_time_t time_now()
{
    return static_cast<highp_time_t>(std::time(nullptr));
}

using std::clamp;
} // namespace yasio
