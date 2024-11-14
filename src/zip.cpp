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

#include "zip.hpp"
#include "mxe_error.hpp"
#include "methylome.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <iostream>
#include <print>
#include <string>

using std::println;
using std::string;

using hr_clock = std::chrono::high_resolution_clock;

namespace po = boost::program_options;
using po::value;

auto
zip_main(int argc, char *argv[]) -> int {
  static const auto description = "zip";

  bool verbose{};
  bool unzip{false};

  uint32_t n_cpgs{};

  string input_file{};
  string index_file{};
  string output_file{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("input,i", value(&input_file)->required(), "input file")
    ("output,o", value(&output_file)->required(),
     "output file")
    ("unzip,u", po::bool_switch(&unzip), "unzip the file")
    ("index,x", value(&index_file), "can give number of cpgs needed to unzip")
    ("n-cpgs,n", value(&n_cpgs), "number of cpgs needed to unzip")
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
    println("{}", e.what());
    desc.print(std::cout);
    return EXIT_FAILURE;
  }

  if (unzip && index_file.empty() && n_cpgs == 0) {
    println(std::cerr, "Index file or number of cpgs needed to unzip");
    return EXIT_FAILURE;
  }

  if (verbose && !index_file.empty() && n_cpgs != 0)
    println(std::cerr, "Only one of n-cpgs and index are needed to unzip");

  if (verbose)
    print("Input: {}\n"
          "Output: {}\n"
          "Unzip: {}\n"
          "N-CpGs: {}\n"
          "Index: {}\n",
          input_file, output_file, unzip, unzip ? std::to_string(n_cpgs) : "NA",
          index_file.empty() ? "NA" : index_file);

  if (unzip && !index_file.empty()) {
    cpg_index index{};
    if (const auto index_read_err = index.read(index_file); index_read_err) {
      println(std::cerr, "Error: {} ({})", index_read_err, index_file);
      return EXIT_FAILURE;
    }
    if (n_cpgs != 0 && n_cpgs != index.n_cpgs_total) {
      println(std::cerr, "Inconsistent n-cpgs given ({} vs. {} in {})", n_cpgs,
              index.n_cpgs_total, index_file);
      return EXIT_FAILURE;
    }
    n_cpgs = index.n_cpgs_total;
  }

  methylome m;
  const auto meth_read_start{hr_clock::now()};
  const auto meth_read_err = m.read(input_file, n_cpgs);
  const auto meth_read_stop{hr_clock::now()};
  if (meth_read_err) {
    println(std::cerr, "Error reading input: {} ({})", input_file,
            meth_read_err);
    return EXIT_FAILURE;
  }
  if (verbose)
    println("Methylome read time: {}s",
            duration(meth_read_start, meth_read_stop));

  const auto meth_write_start{hr_clock::now()};
  const auto meth_write_err = m.write(output_file, !unzip);
  const auto meth_write_stop{hr_clock::now()};
  if (meth_write_err) {
    println(std::cerr, "Error writing output: {} ({})", output_file,
            meth_write_err);
    return EXIT_FAILURE;
  }
  if (verbose)
    println("Methylome write time: {}s",
            duration(meth_write_start, meth_write_stop));

  return EXIT_SUCCESS;
}
