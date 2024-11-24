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

#include "logger.hpp"
#include "methylome.hpp"
#include "mxe_error.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <iostream>
#include <print>
#include <string>
#include <tuple>
#include <vector>

auto
zip_main(int argc, char *argv[]) -> int {
  static constexpr mxe_log_level default_log_level = mxe_log_level::info;
  static constexpr auto command = "zip";

  std::string methylome_input{};
  std::string metadata_input{};
  std::string methylome_output{};
  std::string metadata_output{};
  mxe_log_level log_level{};
  bool unzip{false};

  namespace po = boost::program_options;

  po::options_description desc(std::format("Usage: mxe {} [options]", command));
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("input,i", po::value(&methylome_input)->required(), "input file")
    ("output,o", po::value(&methylome_output)->required(),
     "output file")
    ("unzip,u", po::bool_switch(&unzip), "unzip the file")
    ("meta", po::value(&metadata_input), "metadata input (defaults to input.json)")
    ("meta-out", po::value(&metadata_output),
     "metadata output (defaults to output.json)")
    ("log-level,v", po::value(&log_level)->default_value(default_log_level),
     "log level {debug,info,warning,error,critical}")
    ;
  // clang-format on
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
    std::println("{}", e.what());
    desc.print(std::cout);
    return EXIT_FAILURE;
  }

  auto &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  if (metadata_input.empty())
    metadata_input = std::format("{}.json", methylome_input);

  if (metadata_output.empty())
    metadata_output = std::format("{}.json", methylome_output);

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Input", methylome_input},
    {"Metadata input", metadata_input},
    {"Output", methylome_output},
    {"Metadata output", metadata_output},
    {"Unzip", std::format("{}", unzip)},
    // clang-format on
  };
  log_args<mxe_log_level::info>(args_to_log);

  auto [meta, meta_read_err] = methylome_metadata::read(metadata_input);
  if (meta_read_err) {
    lgr.error("Error reading metadata: {} ({})", meta_read_err, metadata_input);
    return EXIT_FAILURE;
  }

  if (unzip && !meta.is_compressed) {
    lgr.warning("Attempting to unzip but methylome is not zipped");
    return EXIT_FAILURE;
  }

  if (!unzip && meta.is_compressed) {
    lgr.warning("Attempting to zip but methylome is zipped");
    return EXIT_FAILURE;
  }

  methylome meth;
  const auto meth_read_start = std::chrono::high_resolution_clock::now();
  const auto meth_read_err = meth.read(methylome_input, meta);
  const auto meth_read_stop = std::chrono::high_resolution_clock::now();
  if (meth_read_err) {
    lgr.error("Error reading methylome: {} ({})", methylome_input,
              meth_read_err);
    return EXIT_FAILURE;
  }
  lgr.debug("Methylome read time: {}s",
            duration(meth_read_start, meth_read_stop));

  const auto meth_write_start = std::chrono::high_resolution_clock::now();
  const auto meth_write_err = meth.write(methylome_output, !unzip);
  const auto meth_write_stop = std::chrono::high_resolution_clock::now();
  if (meth_write_err) {
    lgr.error("Error writing output: {} ({})", methylome_output,
              meth_write_err);
    return EXIT_FAILURE;
  }
  lgr.debug("Methylome write time: {}s",
            duration(meth_write_start, meth_write_stop));

  meta.is_compressed = !meta.is_compressed;

  if (const auto err = methylome_metadata::write(meta, metadata_output); err) {
    lgr.error("Error writing metadata: {} ({})", err, metadata_output);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
