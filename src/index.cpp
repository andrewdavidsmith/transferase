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

#include "index.hpp"
#include "cpg_index.hpp"
#include "mxe_error.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <print>
#include <string>

using std::println;
using std::string;

namespace chrono = std::chrono;
using hr_clock = chrono::high_resolution_clock;

namespace po = boost::program_options;
using po::value;

auto
index_main(int argc, char *argv[]) -> int {
  static const auto description = "index";

  bool verbose{};

  string genome_file{};
  string index_file{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("genome,g", value(&genome_file)->required(), "genome_file")
    ("index,x", value(&index_file)->required(), "output file")
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
    println(std::cerr, "{}", e.what());
    desc.print(std::cerr);
    return EXIT_FAILURE;
  }

  if (verbose)
    print("genome: {}\n"
          "index: {}\n",
          genome_file, index_file);

  cpg_index index;
  const auto constr_start{hr_clock::now()};
  index.construct(genome_file);
  const auto constr_stop{hr_clock::now()};
  if (verbose)
    println("construction_time: {:.3}s\n"
            "{}",
            duration(constr_start, constr_stop), index);

  if (const auto index_write_err = index.write(index_file); index_write_err) {
    println("Error: {} ({})", index_write_err, index_file);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
