// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Rolo's replacement for yasio's size macros used by Axmol's core.
// Written from scratch for Rolo; it contains no yasio code (see ../../README.md).
#pragma once

// Rounds `size` up to a multiple of `alignment`, which must be a power of two.
// Adding (alignment - 1) then clearing the low bits does that without a division.
#define YASIO_SZ_ALIGN(size, alignment) (((size) + ((alignment) - 1)) & ~((alignment) - 1))
