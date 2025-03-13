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

xfr compress -d methylome_dir -m methylome_name -o output_dir
xfr compress -u -d methylome_dir -m methylome_name -o output_dir
)";

#include "cli_common.hpp"
#include "format_error_code.hpp"  // IWYU pragma: keep
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_metadata.hpp"
#include "utilities.hpp"  // duration()

#include "CLI11/CLI11.hpp"

#include <chrono>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <format>
#include <map>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // IWYU pragma: keep
#include <vector>

auto
command_compress_main(int argc, char *argv[]) -> int {  // NOLINT(*-c-arrays)
  static constexpr auto log_level_default = transferase::log_level_t::info;
  static constexpr auto command = "compress";
  static const auto usage =
    std::format("Usage: xfr {} [options]", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  std::string methylome_dir{};
  std::string methylome_name{};
  std::string methylome_outdir{};
  transferase::log_level_t log_level{log_level_default};
  bool uncompress{false};

  namespace xfr = transferase;

  CLI::App app{about_msg};
  argv = app.ensure_utf8(argv);
  app.usage(usage);
  if (argc >= 2)
    app.footer(description_msg);
  app.get_formatter()->column_width(column_width_default);
  app.get_formatter()->label("REQUIRED", "REQD");
  // clang-format off
  app.add_option("-d,--methylome-dir", methylome_dir, "input methylome directory")
    ->required()
    ->check(CLI::ExistingDirectory);
  app.add_option("-m,--methylome", methylome_name, "methylome name/accession")
    ->required();
  app.add_option("-o,--output-dir", methylome_outdir, "methylome output directory")
    ->required()
    ->check(CLI::ExistingDirectory);
  app.add_option("-u,--uncompress", uncompress, "uncompress the file");
  app.add_option("-v,--log-level", log_level, "{debug, info, warning, error, critical}")
    ->option_text(std::format("ENUM [{}]", log_level_default))
    ->transform(CLI::CheckedTransformer(xfr::str_to_level, CLI::ignore_case));
  // clang-format on

  if (argc < 2) {
    std::println("{}", app.help());
    return EXIT_SUCCESS;
  }

  CLI11_PARSE(app, argc, argv);

  auto &lgr =
    xfr::logger::instance(xfr::shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Methylome input directory", methylome_dir},
    {"Methylome output directory", methylome_outdir},
    {"Methylome name", methylome_name},
    {"Uncompress", std::format("{}", uncompress)},
    // clang-format on
  };
  xfr::log_args<xfr::log_level_t::info>(args_to_log);

  std::error_code ec;
  const auto read_start = std::chrono::high_resolution_clock::now();
  auto meth = xfr::methylome::read(methylome_dir, methylome_name, ec);
  const auto read_stop = std::chrono::high_resolution_clock::now();
  if (ec) {
    lgr.error("Error reading methylome {} {}: {}", methylome_dir,
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
  std::error_code write_err;
  meth.write(methylome_outdir, methylome_name, write_err);
  const auto write_stop = std::chrono::high_resolution_clock::now();
  if (write_err) {
    lgr.error("Error writing output {} {}: {}", methylome_outdir,
              methylome_name, write_err);
    return EXIT_FAILURE;
  }
  lgr.debug("Methylome write time: {}s", duration(write_start, write_stop));

  return EXIT_SUCCESS;
}
