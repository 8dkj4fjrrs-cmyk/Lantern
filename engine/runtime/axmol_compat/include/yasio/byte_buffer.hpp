// Copyright (c) 2026 Oscar Hird
// SPDX-License-Identifier: MIT
//
// Rolo's replacement for yasio's byte buffers, used throughout Axmol's core
// (file contents, decoded images, shader uniforms). Written from scratch for
// Rolo; it contains no yasio code (see ../../README.md).
#pragma once

#include "yasio/pod_vector.hpp"
#include "yasio/string_view.hpp"

namespace yasio {
template <typename T, typename Alloc = buffer_allocator<T>, std::enable_if_t<is_byte_type<T>::value, int> = 0>
using basic_byte_buffer = pod_vector<T, Alloc>;

using sbyte_buffer = basic_byte_buffer<char>;
using byte_buffer = basic_byte_buffer<unsigned char>;
} // namespace yasio
