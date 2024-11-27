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

#include "command_index.hpp"

#include "cpg_index.hpp"
#include "cpg_index_meta.hpp"
#include "logger.hpp"
#include "mxe_error.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <tuple>
#include <vector>

auto
command_index_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "index";

  std::string genome_filename{};
  std::string index_file{};
  std::string metadata_output{};
  mxe_log_level log_level{};

  namespace po = boost::program_options;

  po::options_description desc(std::format("Usage: mxe {} [options]", command));
  desc.add_options()
    // clang-format off
    ("help,h", "produce help message")
    ("genome,g", po::value(&genome_filename)->required(), "genome_file")
    ("index,x", po::value(&index_file)->required(), "output file")
    ("meta", po::value(&metadata_output),
     "metadata output (defaults to output.json)")
    ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
     "log level {debug,info,warning,error,critical}")
    // clang-format on
    ;
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help") || argc == 1) {
      desc.print(std::cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println(std::cerr, "{}", e.what());
    desc.print(std::cerr);
    return EXIT_FAILURE;
  }

  auto &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    lgr.error("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  if (metadata_output.empty())
    metadata_output = get_default_cpg_index_meta_filename(index_file);

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Genome", genome_filename},
    {"Index", index_file},
    {"Index metadata", metadata_output},
    // clang-format on
  };
  log_args<mxe_log_level::info>(args_to_log);

  const auto constr_start = std::chrono::high_resolution_clock::now();
  const auto [index, cim, err] = initialize_cpg_index(genome_filename);
  const auto constr_stop = std::chrono::high_resolution_clock::now();
  lgr.debug("Index construction time: {:.3}s",
            duration(constr_start, constr_stop));
  if (err) {
    lgr.error("Error constructing index: {}", err);
    return EXIT_FAILURE;
  }

  if (const auto write_err = index.write(index_file); write_err) {
    lgr.error("Error writing cpg index {}: {}", index_file, write_err);
    return EXIT_FAILURE;
  }

  if (const auto write_err = cim.write(metadata_output); write_err) {
    lgr.error("Error writing cpg index metadata: {}", write_err);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
