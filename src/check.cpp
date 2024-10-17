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
#include "mc16_error.hpp"
#include "methylome.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

using std::cout;
using std::error_code;
using std::make_pair;
using std::ofstream;
using std::ostream;
using std::pair;
using std::print;
using std::println;
using std::string;
using std::string_view;
using std::tuple;
using std::uint32_t;
using std::vector;

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
  }
  catch (po::error &e) {
    println(cout, "{}", e.what());
    desc.print(cout);
    return EXIT_FAILURE;
  }

  if (verbose)
    print("index: {}\n"
          "methylome: {}\n"
          "output: {}\n",
          index_file, meth_file, output_file);

  cpg_index index{};
  if (const auto cpg_index_err = index.read(index_file); cpg_index_err) {
    println("Error: {} ({})", cpg_index_err, index_file);
    return EXIT_FAILURE;
  }

  if (verbose)
    println("index:\n{}", index);

  methylome meth{};
  const auto meth_read_err = meth.read(meth_file, index.n_cpgs_total);
  if (meth_read_err) {
    println(cout, "Error: {} ({})", meth_read_err, meth_file);
    return EXIT_FAILURE;
  }

  const auto methylome_size = std::size(meth.cpgs);
  const auto check_outcome =
    (methylome_size == index.n_cpgs_total) ? "pass" : "fail";

  const auto total_counts = meth.total_counts();

  std::ofstream of;
  if (!output_file.empty())
    of.open(output_file);
  ostream out(output_file.empty() ? cout.rdbuf() : of.rdbuf());
  if (!out) {
    println(cout, "failed to open output file: {}", output_file);
    return EXIT_FAILURE;
  }

  const double n_reads = total_counts.n_meth + total_counts.n_unmeth;
  const auto mean_meth_weighted = total_counts.n_meth / n_reads;
  const auto sites_covered_fraction =
    static_cast<double>(total_counts.n_covered) / methylome_size;

  print(out,
        "check: {}\n"
        "methylome_size: {}\n"
        "index_size: {}\n"
        "total_counts: {}\n"
        "sites_covered_fraction: {}\n"
        "mean_meth_weighted: {}\n",
        check_outcome, methylome_size, index.n_cpgs_total, total_counts,
        sites_covered_fraction, mean_meth_weighted);

  return EXIT_SUCCESS;
}
