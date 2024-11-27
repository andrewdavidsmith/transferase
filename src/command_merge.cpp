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

#include "command_merge.hpp"

#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_metadata.hpp"
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

[[nodiscard]] static auto
verify_consistent_metadata(const std::vector<std::string> &meth_files)
  -> std::error_code {
  std::vector<methylome_metadata> mms;
  for (const auto &mf : meth_files) {
    const auto filename = get_default_methylome_metadata_filename(mf);
    const auto [mm, mm_err] = methylome_metadata::read(filename);
    if (mm_err)
      return mm_err;
    mms.emplace_back(mm);
  }

  const auto consistent = [&a = mms.back()](const auto &b) -> bool {
    return methylome_metadata_consistent(a, b);
  };

  // ADS: this does an extra comparison of the final element to itself
  return std::ranges::all_of(mms, consistent)
           ? methylome_metadata_error::ok
           : methylome_metadata_error::inconsistent;
}

auto
command_merge_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "merge";

  mxe_log_level log_level{};
  std::string output_file{};
  std::string metadata_output{};

  namespace po = boost::program_options;

  po::options_description desc(std::format("Usage: mxe {} [options]", command));
  desc.add_options()
    // clang-format off
    ("help,h", "produce help message")
    ("output,o", po::value(&output_file)->required(), "output file")
    ("meta,e", po::value(&metadata_output), "output metadata file")
    ("input,i", po::value<std::vector<std::string>>()->multitoken()->required(), "input files")
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

  if (metadata_output.empty())
    metadata_output = get_default_methylome_metadata_filename(output_file);

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
    {"Output metadata", metadata_output},
    {"Number of inputs", std::format("{}", n_inputs)},
    // clang-format on
  };
  for (const auto [i, filename] : std::views::enumerate(input_files))
    args_to_log.emplace_back(std::format("Methylome{}", i), filename);
  log_args<mxe_log_level::info>(args_to_log);

  // ADS: first read the last methylome in the input files list as we only
  // do n-1 merges so we need one to start with; we can't merge an
  // empty methylome, so we need a real one outside the loop
  methylome meth;
  double read_time{};
  {
    const auto filename = input_files.back();
    const auto meta_file = get_default_methylome_metadata_filename(filename);
    const auto [meta, meta_err] = methylome_metadata::read(meta_file);
    if (meta_err) {
      lgr.error("Error reading metadata {}: {}", meta_file, meta_err);
      return EXIT_FAILURE;
    }
    const auto read_start = std::chrono::high_resolution_clock::now();
    const auto meth_err = meth.read(filename, meta);
    const auto read_stop = std::chrono::high_resolution_clock::now();
    read_time += duration(read_start, read_stop);
    if (meth_err) {
      lgr.error("Error reading methylome {}: {}", filename, meth_err);
      return EXIT_FAILURE;
    }
  }

  const auto n_cpgs = size(meth);  // quick consistency check
  double merge_time{};

  // ADS: consider threads here; no reason not to

  // ADS: iterate through the first n-1 methylomes, merge each with the n-th
  for (const auto &filename : input_files | std::views::take(n_inputs - 1)) {
    const auto meta_file = get_default_methylome_metadata_filename(filename);
    const auto [meta, meta_err] = methylome_metadata::read(meta_file);
    if (meta_err) {
      lgr.error("Error reading metadata {}: {}", meta_file, meta_err);
      return EXIT_FAILURE;
    }
    methylome tmp{};
    const auto read_start = std::chrono::high_resolution_clock::now();
    const auto meth_err = tmp.read(filename, meta);
    const auto read_stop = std::chrono::high_resolution_clock::now();
    read_time += duration(read_start, read_stop);
    if (meth_err) {
      lgr.error("Error reading methylome {}: {}", filename, meth_err);
      return EXIT_FAILURE;
    }
    if (size(tmp) != n_cpgs) {
      lgr.error("Wrong methylome size {} (expected {})", size(tmp), n_cpgs);
      return EXIT_FAILURE;
    }
    const auto merge_start = std::chrono::high_resolution_clock::now();
    meth.add(tmp);
    const auto merge_stop = std::chrono::high_resolution_clock::now();
    merge_time += duration(merge_start, merge_stop);
  }

  const auto write_start = std::chrono::high_resolution_clock::now();
  if (const auto meth_write_err = meth.write(output_file); meth_write_err) {
    lgr.error("Error writing methylome {}: {}", output_file, meth_write_err);
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

  auto err = verify_consistent_metadata(input_files);
  if (err) {
    lgr.error("Inconsistent metadata: {}", err);
    return EXIT_FAILURE;
  }

  const auto meta_file =
    get_default_methylome_metadata_filename(input_files[0]);
  auto [meta, meta_err] = methylome_metadata::read(meta_file);
  if (meta_err) {
    lgr.error("Error reading metadata {}: {}", meta_file, meta_err);
    return EXIT_FAILURE;
  }

  if (err = meta.update(output_file); err) {
    lgr.error("Error updating metadata {}: {}", metadata_output, err);
    return EXIT_FAILURE;
  }

  const auto meta_outfile =
    get_default_methylome_metadata_filename(output_file);
  if (err = methylome_metadata::write(meta, meta_outfile); err) {
    lgr.error("Error updating metadata {}: {}", metadata_output, err);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
