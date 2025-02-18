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

#include "counts_file_format.hpp"
#include "counts_file_format_impl.hpp"

#include "zlib_adapter.hpp"

#include <algorithm>
#include <cctype>  // for std::isdigit
#include <cerrno>
#include <charconv>
#include <cmath>     // for std::round
#include <cstdint>   // for std::uint32_t
#include <iterator>  // for std::size
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>

namespace transferase {

[[nodiscard]] auto
parse_counts_line(const std::string &line, std::uint32_t &pos,
                  std::uint32_t &n_meth, uint32_t &n_unmeth) -> bool {
  constexpr auto is_sep = [](const char x) { return x == ' ' || x == '\t'; };

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto c_end = line.data() + std::size(line);
  auto field_s = line.data();

  auto field_e = std::find_if(std::min(field_s + 1, c_end), c_end, is_sep);
  bool failed = (field_e == c_end);
  // [field_s, field_e] = chrom

  // get pos
  auto res = std::from_chars(std::min(field_e + 1, c_end), c_end, pos);
  failed = failed || (res.ptr == c_end);

  // skip strand
  field_s = res.ptr + 1;
  field_e = std::find_if(std::min(field_s, c_end), c_end, is_sep);
  failed = failed || (field_e == c_end);

  // skip context
  field_s = field_e + 1;
  field_e = std::find_if(std::min(field_s, c_end), c_end, is_sep);
  failed = failed || (field_e == c_end);

  // get level
  double meth{};
  field_s = field_e + 1;
  res = std::from_chars(std::min(field_s, c_end), c_end, meth);
  failed = failed || (res.ptr == c_end);

  // get n reads
  std::uint32_t n_reads{};
  field_s = res.ptr + 1;
  res = std::from_chars(std::min(field_s, c_end), c_end, n_reads);
  failed = failed || (res.ec != std::errc{});
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  n_meth = static_cast<std::remove_reference_t<decltype(n_meth)>>(
    std::round(meth * n_reads));
  n_unmeth = n_reads - n_meth;

  return !failed;
}

[[nodiscard]] STATIC auto
is_counts_format(const std::string &filename) -> bool {
  static constexpr auto max_lines_to_read = 10000;

  std::error_code unused_error{};
  gzinfile in(filename, unused_error);
  if (!in)
    return false;

  std::string line;
  std::uint32_t n_lines{};
  while (n_lines++ < max_lines_to_read && getline(in, line) && line[0] == '#')
    ;
  if (!in)
    return false;

  std::istringstream iss(line);

  std::string chrom;
  std::uint32_t pos{};
  char strand{};
  std::string context;
  double meth_level{};
  std::uint32_t n_reads{};
  if (!(iss >> chrom >> pos >> strand >> context >> meth_level >> n_reads))
    return false;
  return true;
}

[[nodiscard]] STATIC auto
is_xcounts_format(const std::string &filename) -> bool {
  static constexpr auto max_lines_to_read = 10000;

  std::error_code unused_error{};
  gzinfile in(filename, unused_error);
  if (!in)
    return false;

  std::string line;
  std::uint32_t n_lines{};
  bool found_chrom{false};
  bool found_coords{false};
  while (n_lines++ < max_lines_to_read && getline(in, line)) {
    if (line[0] == '#')
      continue;
    if (!std::isdigit(line[0])) {  // chrom line
      if (line.find_first_of(" \t") != std::string::npos) {
        return false;
      }
      found_chrom = true;
    }
    else {
      std::istringstream iss(line);
      std::uint32_t pos{}, n_meth{}, n_unmeth{};
      if (!(iss >> pos >> n_meth >> n_unmeth))
        return false;
      found_coords = true;
    }
  }
  return found_chrom && found_coords;
}

[[nodiscard]] auto
get_meth_file_format(const std::string &filename)
  -> std::tuple<counts_file_format, std::error_code> {
  // check file first

  std::error_code ec;
  gzinfile in(filename, ec);
  if (ec)
    return {{}, ec};

  std::string line;
  if (!getline(in, line))
    return {{}, std::make_error_code(std::errc(errno))};

  if (is_counts_format(filename))
    return {counts_file_format::counts, {}};

  if (is_xcounts_format(filename))
    return {counts_file_format::xcounts, {}};

  return {counts_file_format::unknown, {}};
}

}  // namespace transferase
