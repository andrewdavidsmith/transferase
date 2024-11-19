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

#include "utilities.hpp"

/*
  Functions defined here are used by multiple source files
 */

#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "methylome.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <ostream>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

using std::array;
using std::pair;
using std::string;
using std::to_chars;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;

auto
write_bins(std::ostream &out, const uint32_t bin_size, const cpg_index &index,
           const vector<counts_res_cov> &results) -> std::error_code {
  static constexpr auto buf_size{512};
  static constexpr auto delim{'\t'};

  array<char, buf_size> buf{};
  const auto buf_beg = buf.data();
  const auto buf_end = buf.data() + buf_size;

  vector<counts_res_cov>::const_iterator res{cbegin(results)};

  const auto zipped = vs::zip(index.chrom_size, index.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    rg::copy(chrom_name, buf_beg);
    buf[size(chrom_name)] = delim;
    for (uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      const auto bin_end = std::min(bin_beg + bin_size, chrom_size);
      std::to_chars_result tcr{buf_beg + size(chrom_name) + 1, std::errc()};
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
#endif
      tcr = to_chars(tcr.ptr, buf_end, bin_beg);
      *tcr.ptr++ = delim;
      tcr = to_chars(tcr.ptr, buf_end, bin_end);
      *tcr.ptr++ = delim;
      tcr = to_chars(tcr.ptr, buf_end, res->n_meth);
      *tcr.ptr++ = delim;
      tcr = to_chars(tcr.ptr, buf_end, res->n_unmeth);
      *tcr.ptr++ = delim;
      tcr = to_chars(tcr.ptr, buf_end, res->n_covered);
      *tcr.ptr++ = '\n';
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif
      out.write(buf_beg, rg::distance(buf_beg, tcr.ptr));
      if (!out)
        return std::make_error_code(std::errc(errno));
      ++res;
    }
  }
  assert(res == cend(results));
  return {};
}

auto
get_mxe_config_dir_default(std::error_code &ec) -> std::string {
  static const auto config_dir_rhs = std::filesystem::path(".config/mxe");
  static const auto env_home = std::getenv("HOME");
  if (!env_home) {
    ec = std::make_error_code(std::errc{errno});
    return {};
  }
  const std::filesystem::path config_dir = env_home / config_dir_rhs;
  return config_dir;
}

/*
auto
check_mxe_config_dir(const std::string &dirname, std::error_code &ec) -> bool {
  const bool exists = std::filesystem::exists(dirname, ec);
  if (ec)
    return false;

  if (!exists) {
    ec = std::make_error_code(std::errc::no_such_file_or_directory);
    return false;
  }

  const bool is_dir = std::filesystem::is_directory(dirname, ec);
  if (ec)
    return false;
  if (!is_dir) {
    ec = std::make_error_code(std::errc::not_a_directory);
    return false;
  }

  return true;
}
*/
