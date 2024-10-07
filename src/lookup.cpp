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

#include "lookup.hpp"
#include "cpg_index.hpp"
#include "genomic_interval.hpp"
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

namespace vs = std::views;

namespace po = boost::program_options;
using po::value;

namespace chrono = std::chrono;
using hr_clock = chrono::high_resolution_clock;

// ADS: multiply defined
[[nodiscard]] static inline auto
duration(const auto start, const auto stop) {
  return chrono::duration_cast<chrono::duration<double>>(stop - start).count();
}

auto
lookup_main(int argc, char *argv[]) -> int {
  static const auto description = "lookup";

  bool verbose{};

  string index_file{};
  string intervals_file{};
  string meth_file{};
  string output_file{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("index,x", value(&index_file)->required(), "index file")
    ("intervals,i", value(&intervals_file)->required(), "intervals file")
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
          "intervals: {}\n"
          "methylome: {}\n"
          "output: {}\n",
          index_file, intervals_file, meth_file, output_file);

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

  const auto gis = genomic_interval::load(index, intervals_file);
  if (gis.empty()) {
    println(cerr, "failed to read intervals file: {}", intervals_file);
    return EXIT_FAILURE;
  }
  const auto lookup_start{hr_clock::now()};

  const vector<pair<uint32_t, uint32_t>> offsets = index.get_offsets(gis);
  const auto results = meth.get_counts(offsets);

  const auto lookup_stop{hr_clock::now()};
  if (verbose)
    println("lookup_time: {:.3}s", duration(lookup_start, lookup_stop));

  ofstream out(output_file);
  if (!out) {
    println(cerr, "failed to open output file: {}", output_file);
    return EXIT_FAILURE;
  }

  for (const auto [gi, res] : vs::zip(gis, results))
    println(out, "{}\t{}\t{}\t{}", gi, res.n_meth, res.n_unmeth, res.n_covered);

  return EXIT_SUCCESS;
}
