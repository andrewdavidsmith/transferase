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

#include "command_compress.hpp"

static constexpr auto about = R"(
make the methylome data file smaller
)";

static constexpr auto description = R"(
The compress command is primarily used to prepare data for use by the
server when space is at a premium. The compress command makes a
methylome data file smaller. The compression format is custome and can
only be decompressed with this command. Compared to gzip, this command
is roughly 4-5x faster, with a cost of 1.2x in size, and decompress
slightly faster. The compression status is not encoded in the
methylome data files, but in the metadata files, so be careful not to
confuse the methylome metadata files for original and compressed
files.
)";

static constexpr auto examples = R"(
Examples:

xfrase compress -o compressed.m16 -i original.m16
xfrase compress -u -o original.m16 -i compressed.m16
)";

#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_metadata.hpp"
#include "utilities.hpp"     // duration()
#include "xfrase_error.hpp"  // IWYU pragma: keep

#include <boost/program_options.hpp>

#include <chrono>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // IWYU pragma: keep
#include <vector>

auto
command_compress_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "compress";
  static const auto usage =
    std::format("Usage: xfrase {} [options]\n", strip(command));
  static const auto about_msg =
    std::format("xfrase {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  std::string methylome_input{};
  std::string metadata_input{};
  std::string methylome_output{};
  std::string metadata_output{};
  xfrase_log_level log_level{};
  bool uncompress{false};

  namespace po = boost::program_options;

  po::options_description desc("Options");
  // clang-format off
  desc.add_options()
    ("help,h", "print this message and exit")
    ("input,i", po::value(&methylome_input)->required(), "input file")
    ("output,o", po::value(&methylome_output)->required(),
     "output file")
    ("uncompress,u", po::bool_switch(&uncompress), "uncompress the file")
    ("meta", po::value(&metadata_input), "metadata input (default: input.json)")
    ("meta-out", po::value(&metadata_output),
     "metadata output (default: output.json)")
    ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
     "log level {debug,info,warning,error,critical}")
    ;
  // clang-format on
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help") || argc == 1) {
      std::println("{}\n{}", about_msg, usage);
      desc.print(std::cout);
      std::println("\n{}", description_msg);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    std::println("{}\n{}", about_msg, usage);
    desc.print(std::cout);
    std::println("\n{}", description_msg);
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
    {"Uncompress", std::format("{}", uncompress)},
    // clang-format on
  };
  log_args<xfrase_log_level::info>(args_to_log);

  auto [meta, meta_read_err] = methylome_metadata::read(metadata_input);
  if (meta_read_err) {
    lgr.error("Error reading metadata {}: {}", metadata_input, meta_read_err);
    return EXIT_FAILURE;
  }

  if (uncompress && !meta.is_compressed) {
    lgr.warning("Attempting to uncompress but methylome is not compressed");
    return EXIT_FAILURE;
  }

  if (!uncompress && meta.is_compressed) {
    lgr.warning("Attempting to compress but methylome is compressed");
    return EXIT_FAILURE;
  }

  const auto meth_read_start = std::chrono::high_resolution_clock::now();
  const auto [meth, meth_read_err] = methylome::read(methylome_input, meta);
  const auto meth_read_stop = std::chrono::high_resolution_clock::now();
  if (meth_read_err) {
    lgr.error("Error reading methylome {}: {}", methylome_input, meth_read_err);
    return EXIT_FAILURE;
  }
  lgr.debug("Methylome read time: {}s",
            duration(meth_read_start, meth_read_stop));

  const auto meth_write_start = std::chrono::high_resolution_clock::now();
  if (const auto meth_write_err = meth.write(methylome_output, !uncompress);
      meth_write_err) {
    lgr.error("Error writing output {}: {}", methylome_output, meth_write_err);
    return EXIT_FAILURE;
  }
  const auto meth_write_stop = std::chrono::high_resolution_clock::now();
  lgr.debug("Methylome write time: {}s",
            duration(meth_write_start, meth_write_stop));

  meta.is_compressed = !meta.is_compressed;

  if (const auto meta_write_err = meta.write(metadata_output); meta_write_err) {
    lgr.error("Error writing metadata {}: {}", metadata_output, meta_write_err);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
