// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Lantern's replacement for yasio's object-pool macros, used by Axmol's
// SimpleTimer. Written from scratch for Lantern; it contains no yasio code
// (see ../../README.md).
//
// The macros let a class allocate its objects from a pool instead of the
// general heap. That is only an optimisation: here they expand to nothing, so
// those classes use ordinary new and delete, which behave the same.
#pragma once

#define DEFINE_OBJECT_POOL_ALLOCATION(ELEMENT_TYPE, ELEMENT_COUNT)
#define DEFINE_FAST_OBJECT_POOL_ALLOCATION(ELEMENT_TYPE, ELEMENT_COUNT)
#define DEFINE_CONCURRENT_OBJECT_POOL_ALLOCATION(ELEMENT_TYPE, ELEMENT_COUNT)
