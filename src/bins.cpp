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

#include "bins.hpp"
#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "methylome.hpp"

#include <boost/program_options.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <tuple>
#include <vector>
#include <cstdint>

using std::array;
using std::cbegin;
using std::cend;
using std::cerr;
using std::cout;
using std::min;
using std::ofstream;
using std::print;
using std::println;
using std::string;
using std::tuple;
using std::uint32_t;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;

namespace po = boost::program_options;
using po::value;

static auto
bin_counts(cpg_index::vec::const_iterator &posn_itr,
           const cpg_index::vec::const_iterator posn_end,
           const uint32_t bin_end, methylome::vec::const_iterator &cpg_itr)
  -> counts_res {
  uint32_t n_meth{};
  uint32_t n_unmeth{};
  uint32_t n_covered{};
  while (posn_itr != posn_end && *posn_itr < bin_end) {
    n_meth += cpg_itr->first;
    n_unmeth += cpg_itr->second;
    n_covered += (cpg_itr->first + cpg_itr->second > 0);
    ++cpg_itr;
    ++posn_itr;
  }
  return {n_meth, n_unmeth, n_covered};
}

static auto
write_bins(std::ostream &out, const uint32_t bin_size,
           const cpg_index &index, const vector<counts_res> &results) -> void {
  static constexpr auto buf_size{512};
  static constexpr auto delim{'\t'};

  array<char, buf_size> buf{};
  const auto buf_beg = buf.data();
  const auto buf_end = buf.data() + buf_size;

  vector<counts_res>::const_iterator res{cbegin(results)};

  const auto zipped = vs::zip(index.chrom_size, index.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    rg::copy(chrom_name, buf_beg);
    buf[size(chrom_name)] = delim;
    for (uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      const auto bin_end = min(bin_beg + bin_size, chrom_size);
      std::to_chars_result tcr{buf_beg + size(chrom_name) + 1, std::errc()};
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
      tcr = std::to_chars(tcr.ptr, buf_end, bin_beg);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, bin_end);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res->n_meth);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res->n_unmeth);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res->n_covered);
      *tcr.ptr++ = '\n';
#pragma GCC diagnostic push
      out.write(buf_beg, rg::distance(buf_beg, tcr.ptr));
      ++res;
    }
  }
  assert(res == cend(results));
}

auto
bins_main(int argc, char *argv[]) -> int {
  static const auto description = std::format("{}", "bins");

  bool verbose{};

  string index_file{};
  string meth_file{};
  string output_file{};

  uint32_t bin_size{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("index,x", value(&index_file)->required(), "index file")
    ("bin,b", value(&bin_size)->required(), "size of bins")
    ("meth,m", value(&meth_file)->required(), "methylation file")
    ("output,o", value(&output_file)->required(), "output file")
    ("verbose,v", po::bool_switch(&verbose), "print more run info")
    ;
  // clang-format on
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
      desc.print(std::cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    println(cerr, "{}", e.what());
    desc.print(std::cout);
    return EXIT_FAILURE;
  }

  if (verbose)
    print("index: {}\n"
          "methylome: {}\n"
          "output: {}\n"
          "bin_size: {}\n",
          index_file, meth_file, output_file, bin_size);

  cpg_index index{};
  if (index.read(index_file) != 0) {
    println(cerr, "failed to read cpg index: {}", index_file);
    return EXIT_FAILURE;
  }

  if (verbose)
    println("index:\n{}", index);

  methylome meth{};
  if (meth.read(meth_file, index.n_cpgs_total) != 0) {
    println(cerr, "failed to read methylome: {}", meth_file);
    return EXIT_FAILURE;
  }

  ofstream out(output_file);
  if (!out) {
    println(cerr, "failed to open output file: {}", output_file);
    return EXIT_FAILURE;
  }

  vector<counts_res> results;
  string chrom_name;
  const auto zipped =
    vs::zip(index.positions, index.chrom_size, index.chrom_offset);
  for (const auto [positions, chrom_size, offset] : zipped) {
    auto posn_itr = cbegin(positions);
    const auto posn_end = cend(positions);
    auto cpg_itr = cbegin(meth.cpgs) + offset;
    for (uint32_t i = 0; i < chrom_size; i += bin_size) {
      const auto bin_end = min(i + bin_size, chrom_size);
      results.emplace_back(bin_counts(posn_itr, posn_end, bin_end, cpg_itr));
    }
  }

  write_bins(out, bin_size, index, results);

  return EXIT_SUCCESS;
}
