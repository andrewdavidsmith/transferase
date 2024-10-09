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

#include "merge.hpp"

#include "methylome.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>
#include <ranges>

using std::cout;
using std::error_code;
using std::make_pair;
using std::ofstream;
using std::ostream;
using std::pair;
using std::print;
using std::println;
using std::string;
using std::tuple;
using std::uint32_t;
using std::vector;
using std::size;

namespace fs = std::filesystem;
namespace vs = std::views;

namespace chrono = std::chrono;
namespace po = boost::program_options;
using po::value;

using hr_clock = chrono::high_resolution_clock;

auto
merge_main(int argc, char *argv[]) -> int {
  static const auto description = "merge";

  bool verbose{};

  string output_file{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("output,o", value(&output_file)->required(), "output file")
    ("input,i", po::value<vector<string>>()->multitoken(), "input files")
    ("verbose,v", po::bool_switch(&verbose), "print more run info")
    ;
  // clang-format on
  po::variables_map vm;
  try {
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

  const auto input_files = vm["input"].as<vector<string>>();
  const auto n_inputs = size(input_files);

  if (verbose) {
    print("output: {}\n"
          "input files: {}\n",
          output_file, n_inputs);
    for (const auto &i: input_files) println("{}", i);
  }

  const auto last_file = input_files.back();
  const auto methylome_read_start{hr_clock::now()};
  methylome meth{};
  if (meth.read(last_file) != 0) {
    println(cout, "failed to read methylome: {}", last_file);
    return EXIT_FAILURE;
  }
  double total_read_time = duration(methylome_read_start, hr_clock::now());

  const auto n_cpgs = size(meth);

  double total_merge_time{};

  for (const auto &filename: input_files | vs::take(n_inputs - 1)) {
    methylome tmp{};
    const auto methylome_read_start{hr_clock::now()};
    const auto ret = tmp.read(filename);
    total_read_time += duration(methylome_read_start, hr_clock::now());
    if (ret != 0) {
      println(cout, "failed to read methylome: {}", filename);
      return EXIT_FAILURE;
    }
    if (size(tmp) != n_cpgs) {
      println("wrong methylome size: {} (expected: {})", size(tmp), n_cpgs);
      return EXIT_FAILURE;
    }
    const auto methylome_merge_start{hr_clock::now()};
    meth += tmp;
    total_merge_time += duration(methylome_merge_start, hr_clock::now());
  }

  const auto methylome_write_start{hr_clock::now()};
  if (meth.write(output_file) != 0) {
    println(cout, "failed to write methylome to file: {}", output_file);
    return EXIT_FAILURE;
  }
  const auto total_write_time = duration(methylome_write_start, hr_clock::now());

  if (verbose)
    println("total read time: {:.3}s\n"
            "total merge time: {:.3}s\n"
            "total write time: {:.3}s",
            total_read_time, total_merge_time, total_write_time);

  return EXIT_SUCCESS;
}
