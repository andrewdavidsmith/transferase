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

#include "logger.hpp"
#include "methylome.hpp"
#include "mxe_error.hpp"
#include "utilities.hpp"

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

auto
merge_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "merge";

  mxe_log_level log_level{};
  std::string output_file{};

  namespace po = boost::program_options;

  po::options_description desc(std::format("Usage: mxe {} [options]", command));
  desc.add_options()
    // clang-format off
    ("help,h", "produce help message")
    ("output,o", po::value(&output_file)->required(), "output file")
    ("input,i", po::value<std::vector<std::string>>()->multitoken(), "input files")
    ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
     "log level {debug,info,warning,error,critical}")
    // clang-format on
    ;
  po::variables_map vm;
  try {
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

  logger &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  const auto input_files = vm["input"].as<std::vector<std::string>>();
  const auto n_inputs = std::size(input_files);

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Output", output_file},
    {"Number of inputs", std::format("{}", n_inputs)},
    // clang-format on
  };
  for (const auto [i, filename] : std::views::enumerate(input_files))
    args_to_log.emplace_back(std::format("Methylome{}", i), filename);
  log_args<mxe_log_level::info>(args_to_log);

  const auto last_file = input_files.back();
  const auto last_file_meta =
    std::format("{}.{}", std::filesystem::path(last_file).stem(),
                methylome_metadata::filename_extension);

  auto [meta, meta_read_err] = methylome_metadata::read(last_file_meta);
  if (meta_read_err) {
    lgr.error("Error reading metadata {}: {}", last_file_meta, meta_read_err);
    return EXIT_FAILURE;
  }

  methylome meth{};
  auto read_start = std::chrono::high_resolution_clock::now();
  auto meth_read_err = meth.read(last_file, meta);
  auto read_stop = std::chrono::high_resolution_clock::now();
  double read_time = duration(read_start, read_stop);
  if (meth_read_err) {
    lgr.error("Error reading methylome {}: {}", last_file, meth_read_err);
    return EXIT_FAILURE;
  }

  const auto n_cpgs = size(meth);

  double merge_time{};

  for (const auto &filename : input_files | std::views::take(n_inputs - 1)) {
    const auto meta_file =
      std::format("{}.{}", std::filesystem::path(filename).stem(),
                  methylome_metadata::filename_extension);
    std::tie(meta, meta_read_err) = methylome_metadata::read(meta_file);
    if (meta_read_err) {
      lgr.error("Error reading metadata {}: {}", meta_file, meta_read_err);
      return EXIT_FAILURE;
    }
    methylome tmp{};
    read_start = std::chrono::high_resolution_clock::now();
    meth_read_err = tmp.read(filename, meta);
    read_stop = std::chrono::high_resolution_clock::now();
    read_time += duration(read_start, read_stop);
    if (meth_read_err) {
      lgr.error("Error reading methylome {}: {}", filename, meth_read_err);
      return EXIT_FAILURE;
    }
    if (size(tmp) != n_cpgs) {
      lgr.error("Error: wrong methylome size: {} (expected: {})", size(tmp),
                n_cpgs);
      return EXIT_FAILURE;
    }
    const auto merge_start = std::chrono::high_resolution_clock::now();
    meth.add(tmp);
    const auto merge_stop = std::chrono::high_resolution_clock::now();
    merge_time += duration(merge_start, merge_stop);
  }

  const auto write_start = std::chrono::high_resolution_clock::now();
  if (const auto meth_write_error = meth.write(output_file); meth_write_error) {
    lgr.error("Error writing methylome {}: {}", output_file, meth_write_error);
    return EXIT_FAILURE;
  }
  const auto write_stop = std::chrono::high_resolution_clock::now();
  const auto write_time = duration(write_start, write_stop);

  std::vector<std::tuple<std::string, std::string>> timing_to_log{
    // clang-format off
    {"read time", std::format("{:.3}s", read_time)},
    {"merge time", std::format("{:.3}s", merge_time)},
    {"write time", std::format("{:.3}s", write_time)},
    // clang-format on
  };
  log_args<mxe_log_level::debug>(timing_to_log);

  return EXIT_SUCCESS;
}
