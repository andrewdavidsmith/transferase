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

xfrase compress -d methylome_dir -m methylome_name -o output_dir
xfrase compress -u -d methylome_dir -m methylome_name -o output_dir
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

  std::string methylome_directory{};
  std::string methylome_name{};
  std::string methylome_outdir{};
  transferase::log_level_t log_level{};
  bool uncompress{false};

  namespace po = boost::program_options;

  po::options_description desc("Options");
  // clang-format off
  desc.add_options()
    ("help,h", "print this message and exit")
    ("directory,d", po::value(&methylome_directory)->required(), "input methylome directory")
    ("methylome,m", po::value(&methylome_name)->required(), "methylome name/accession")
    ("output,o", po::value(&methylome_outdir)->required(),
     "methylome output directory")
    ("uncompress,u", po::bool_switch(&uncompress), "uncompress the file")
    ("log-level,v", po::value(&log_level)->default_value(transferase::logger::default_level),
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

  auto &lgr = transferase::logger::instance(transferase::shared_from_cout(),
                                            command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Methylome input directory", methylome_directory},
    {"Methylome output directory", methylome_outdir},
    {"Methylome name", methylome_name},
    {"Uncompress", std::format("{}", uncompress)},
    // clang-format on
  };
  transferase::log_args<transferase::log_level_t::info>(args_to_log);

  std::error_code ec;
  const auto read_start = std::chrono::high_resolution_clock::now();
  auto meth =
    transferase::methylome::read(methylome_directory, methylome_name, ec);
  const auto read_stop = std::chrono::high_resolution_clock::now();
  if (ec) {
    lgr.error("Error reading methylome {} {}: {}", methylome_directory,
              methylome_name, ec);
    return EXIT_FAILURE;
  }
  lgr.debug("Methylome read time: {}s", duration(read_start, read_stop));

  if (uncompress && !meth.meta.is_compressed) {
    lgr.warning("Attempting to uncompress but methylome is not compressed");
    return EXIT_FAILURE;
  }

  if (!uncompress && meth.meta.is_compressed) {
    lgr.warning("Attempting to compress but methylome is compressed");
    return EXIT_FAILURE;
  }

  meth.meta.is_compressed = !uncompress;

  const auto write_start = std::chrono::high_resolution_clock::now();
  const auto write_err = meth.write(methylome_outdir, methylome_name);
  const auto write_stop = std::chrono::high_resolution_clock::now();
  if (write_err) {
    lgr.error("Error writing output {} {}: {}", methylome_outdir,
              methylome_name, write_err);
    return EXIT_FAILURE;
  }
  lgr.debug("Methylome write time: {}s", duration(write_start, write_stop));

  return EXIT_SUCCESS;
}
