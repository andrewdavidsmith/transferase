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

#ifndef LIB_COUNTS_FILE_FORMAT_IMPL_HPP_
#define LIB_COUNTS_FILE_FORMAT_IMPL_HPP_

#ifdef UNIT_TEST
#define STATIC
#define INLINE
#else
#define STATIC static
#define INLINE inline
#endif

#include <string>

namespace transferase {

[[nodiscard]] STATIC auto
is_counts_format(const std::string &filename) -> bool;

[[nodiscard]] STATIC auto
is_xcounts_format(const std::string &filename) -> bool;

}  // namespace transferase

#endif  // LIB_COUNTS_FILE_FORMAT_IMPL_HPP_
