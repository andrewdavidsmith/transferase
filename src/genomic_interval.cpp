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

#include "genomic_interval.hpp"

#include "cpg_index.hpp"
#include "mxe_error.hpp"

#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using std::istream;
using std::string;
using std::tuple;
using std::uint32_t;
using std::vector;

[[nodiscard]] static auto
parse(const cpg_index &index, const string &s,
      std::error_code &ec) -> genomic_interval {
  std::istringstream iss(s);
  genomic_interval gi;
  string tmp;
  if (!(iss >> tmp >> gi.start >> gi.stop)) {
    ec = genomic_interval_code::error_parsing_bed_line;
    return gi;
  }
  const auto ch_id_itr = index.chrom_index.find(tmp);
  if (ch_id_itr == cend(index.chrom_index)) {
    ec = genomic_interval_code::chrom_name_not_found_in_index;
    return gi;
  }
  gi.ch_id = ch_id_itr->second;

  if (gi.stop > index.chrom_size[gi.ch_id])
    ec = genomic_interval_code::interval_past_chrom_end_in_index;

  return gi;
}

[[nodiscard]] auto
genomic_interval::load(const cpg_index &index,
                       const string &filename) -> genomic_interval_load_ret {
  std::ifstream in{filename};
  if (!in)
    return {{}, std::make_error_code(std::errc(errno))};
  vector<genomic_interval> v;
  string line;
  std::error_code ec;
  while (getline(in, line)) {
    const auto gi = parse(index, line, ec);
    if (ec)
      return {{}, ec};
    v.push_back(std::move(gi));
  }
  return {v, genomic_interval_code::ok};
}
