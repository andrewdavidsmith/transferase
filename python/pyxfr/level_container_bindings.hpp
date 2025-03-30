/* MIT License
 *
 * Copyright (c) 2024 Andrew D Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PYTHON_PYXFR_LEVEL_CONTAINER_BINDINGS_HPP_
#define PYTHON_PYXFR_LEVEL_CONTAINER_BINDINGS_HPP_

#include <nanobind/nanobind.h>

namespace transferase {
struct level_element_covered_t;
struct level_element_t;
template <typename level_element_type> struct level_container_md;
}  // namespace transferase

auto
level_container_bindings(nanobind::class_<transferase::level_container_md<
                           transferase::level_element_t>> &cls) -> void;

auto
level_container_covered_bindings(
  nanobind::class_<
    transferase::level_container_md<transferase::level_element_covered_t>> &cls)
  -> void;

#endif  // PYTHON_PYXFR_LEVEL_CONTAINER_BINDINGS_HPP_
