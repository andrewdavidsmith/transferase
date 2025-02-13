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

#include "find_path_to_binary.hpp"

#if defined(__linux__)
#include <limits.h>  // IWYU pragma: keep
#include <unistd.h>
#elif defined(__APPLE__)
#include <libproc.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <iostream>
#include <windows.h>
#endif

#include <array>
#include <string>
#include <sys/types.h>  // for ssize_t

namespace transferase {

[[nodiscard]] auto
find_path_to_binary() -> std::string {
  static constexpr auto path_buf_len = 1024;
  std::array<char, path_buf_len> path_buf{0};  // init to zeros

#if defined(__linux__)
  const ssize_t length =
    readlink("/proc/self/exe", path_buf.data(), path_buf_len - 1);
  if (length != -1)
    return std::string{path_buf.data()};
#elif defined(__APPLE__)
  const pid_t pid = getpid();
  const ssize_t length = proc_pidpath(pid, path_buf.data(), path_buf_len);
  if (length > 0)
    return std::string{path_buf.data()};
#elif defined(_WIN32)
  const DWORD size = GetModuleFileName(nullptr, path_buf.data(), path_buf_len);
  if (size > 0)
    return std::string{path_buf.data()};
#else
  (void)path_buf;
#endif
  return std::string{};
}

}  // namespace transferase
