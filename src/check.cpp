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

#include "check.hpp"
#include "cpg_index.hpp"
#include "methylome.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <filesystem>
#include <system_error>

using std::cerr;
using std::make_pair;
using std::ofstream;
using std::pair;
using std::print;
using std::println;
using std::string;
using std::string_view;
using std::tuple;
using std::uint32_t;
using std::vector;
using std::error_code;

namespace vs = std::views;
namespace fs = std::filesystem;

namespace po = boost::program_options;
using po::value;

using hr_clock = std::chrono::high_resolution_clock;

auto
check_main(int argc, char *argv[]) -> int {
  static const auto description = "check";

  bool verbose{};

  string index_file{};
  string meth_file{};
  string output_file{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("index,x", value(&index_file)->required(), "index file")
    ("methylome,m", value(&meth_file)->required(), "methylome file")
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
  } catch (po::error &e) {
    println(cerr, "{}", e.what());
    desc.print(cerr);
    return EXIT_FAILURE;
  }

  if (verbose)
    print("index: {}\n"
          "methylome: {}\n"
          "output: {}\n",
          index_file, meth_file, output_file);

  cpg_index index{};
  if (index.read(index_file) != 0) {
    println(cerr, "failed to read cpg index: {}", index_file);
    return EXIT_FAILURE;
  }

  if (verbose)
    println("index:\n{}", index);

  error_code errc;
  const auto methylome_size =
    fs::file_size(meth_file, errc)/methylome::record_size;
  if (errc) {
    println("Error obtaining methylome size from file: {}", meth_file);
    return EXIT_FAILURE;
  }

  // total coverage; sites covered; mean methylation; weighted mean
  // methylation

  methylome meth{};
  if (meth.read(meth_file, index.n_cpgs_total) != 0) {
    println(cerr, "failed to read methylome: {}", meth_file);
    return EXIT_FAILURE;
  }

  const auto total_counts = meth.total_counts();

  println("methylome_size: {}", methylome_size);
  println("index_size: {}", index.n_cpgs_total);
  println("total_counts: {}", total_counts);
  println("sites_covered_fraction: {}",
          static_cast<double>(total_counts.n_covered) / methylome_size);
  println("mean_meth_weighted: {}",
          static_cast<double>(total_counts.n_meth) /
          (total_counts.n_meth + total_counts.n_unmeth));

  ofstream out(output_file);
  if (!out) {
    println(cerr, "failed to open output file: {}", output_file);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
