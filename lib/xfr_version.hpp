/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#ifndef LIB_XFR_VERSION_HPP_
#define LIB_XFR_VERSION_HPP_

#include <charconv>   // std::from_chars
#include <cstdint>    // std::uint8_t, etc.
#include <iterator>   // std::iterator_traits
#include <stdexcept>  // std::invalid_argument
#include <string_view>

// Encode version as 0xMMmmPPPP
[[nodiscard]] inline auto
encode_version(const std::uint8_t major, const std::uint8_t minor,
               const std::uint16_t patch) -> std::uint32_t {
  static constexpr auto major_bits_shift = 24;
  static constexpr auto minor_bits_shift = 16;
  return ((static_cast<std::uint32_t>(major) << major_bits_shift) |
          (static_cast<std::uint32_t>(minor) << minor_bits_shift) |
          static_cast<std::uint32_t>(patch));
}

// Decode 0xMMmmPPPP back to major, minor, patch
inline auto
decode_version(const std::uint32_t version, std::uint8_t &major,
               std::uint8_t &minor, std::uint16_t &patch) {
  static constexpr std::uint32_t major_bits_shift = 24;
  static constexpr std::uint32_t minor_bits_shift = 16;
  static constexpr std::uint32_t byte_mask = 0xFF;
  static constexpr std::uint32_t patch_mask = 0xFFFF;

  major = static_cast<std::uint8_t>((version >> major_bits_shift) & byte_mask);
  minor = static_cast<std::uint8_t>((version >> minor_bits_shift) & byte_mask);
  patch = static_cast<std::uint16_t>(version & patch_mask);
}

template <typename It>
[[nodiscard]] auto
parse_version(It ver_beg, It ver_end, std::uint8_t &major, std::uint8_t &minor,
              std::uint16_t &patch) -> bool {
  using CharT = typename std::iterator_traits<It>::value_type;
  static_assert(std::is_same_v<CharT, char>, "Only char iterators supported");

  const auto skip = [&](const char c) {
    if (ver_beg != ver_end && *ver_beg == c)
      ++ver_beg;
  };

  auto parse_uint = [&](auto &out) -> bool {
    const char *first = &*ver_beg;
    const char *last = &*ver_end;
    const auto result = std::from_chars(first, last, out);
    if (result.ec != std::errc{})
      return false;
    ver_beg += (result.ptr - first);
    return true;
  };

  skip('v');  // optional leading 'v'

  std::uint32_t temp_major = 0, temp_minor = 0, temp_patch = 0;
  if (!parse_uint(temp_major))
    return false;
  skip('.');
  if (!parse_uint(temp_minor))
    return false;
  skip('.');
  if (!parse_uint(temp_patch))
    return false;

  // Validate ranges
  if (temp_major > 0xFF || temp_minor > 0xFF || temp_patch > 0xFFFF)
    return false;

  major = static_cast<std::uint8_t>(temp_major);
  minor = static_cast<std::uint8_t>(temp_minor);
  patch = static_cast<std::uint16_t>(temp_patch);
  return true;
}

#endif  // LIB_XFR_VERSION_HPP_
