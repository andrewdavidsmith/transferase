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
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <tuple>
#include <vector>

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
using hr_clock = std::chrono::high_resolution_clock;

namespace po = boost::program_options;
using po::value;

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
    println(cerr, "Failed to read methylome: {}", meth_file);
    return EXIT_FAILURE;
  }

  ofstream out(output_file);
  if (!out) {
    println(cerr, "Failed to open output file: {}", output_file);
    return EXIT_FAILURE;
  }

  const auto get_bins_start{hr_clock::now()};
  const vector<counts_res> results = meth.get_bins(bin_size, index);
  const auto get_bins_stop{hr_clock::now()};

  if (verbose)
    println("Elapsed time to get bins: {:.3}s",
            duration(get_bins_start, get_bins_stop));

  write_bins(out, bin_size, index, results);

  return EXIT_SUCCESS;
}
