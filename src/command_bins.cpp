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

#include "command_bins.hpp"

static constexpr auto about = R"(
summarize methylation levels in non-overlapping genomic bins
)";

static constexpr auto description = R"(
The bins command accepts a bin size and a methylome, and it
generates a summary of the methylation levels in each non-overlapping
bin of the given size. This command runs in two modes, local and
remote. The local mode is for analyzing data on your local storage:
either your own data or data that you downloaded. The remote mode is
for analyzing methylomes in a remote database on a server. Depending
on the mode you select, the options you must specify will differ.
)";

static constexpr auto examples = R"(
Examples:

xfrase bins -x index_dir -g hg38 -s example.com -m SRX012345 \
    -o output.bed -b 1000

xfrase bins --local -d methylome_dir -x index_dir -g hg38 \
    -m methylome_name -o output.bed -b 1000
)";

#include "arguments.hpp"
#include "genome_index.hpp"
#include "genomic_interval_output.hpp"
#include "level_container.hpp"
#include "level_element.hpp"
#include "logger.hpp"
#include "methylome_resource.hpp"
#include "request.hpp"
#include "request_type_code.hpp"
#include "utilities.hpp"

#include <boost/describe.hpp>
#include <boost/program_options.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <format>
#include <iterator>  // for std::cbegin, std::cend
#include <print>
#include <ranges>  // for std::views
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // for std::tuple
#include <vector>

namespace transferase {

struct bins_argset : argset_base<bins_argset> {
  static constexpr auto default_config_filename =
    "transferase_client_config.toml";

  static auto
  get_default_config_file_impl() -> std::string {
    std::error_code ec;
    const auto config_dir = get_transferase_config_dir_default(ec);
    if (ec)
      return {};
    return std::filesystem::path{config_dir} / default_config_filename;
  }

  static constexpr auto hostname_default{""};
  static constexpr auto port_default{"5000"};
  static constexpr auto log_level_default{log_level_t::info};
  std::string hostname;
  std::string port;
  std::string methylome_dir;
  std::string index_dir;
  std::string log_filename;
  transferase::log_level_t log_level{};

  bool local_mode{};
  std::uint32_t bin_size{};
  std::string methylome_names{};
  std::string genome_name{};
  bool write_scores{};
  bool count_covered{};
  std::string output_file{};

  auto
  log_options_impl() const {
    transferase::log_args<transferase::log_level_t::info>(
      std::vector<std::tuple<std::string, std::string>>{
        // clang-format off
        {"hostname", std::format("{}", hostname)},
        {"port", std::format("{}", port)},
        {"methylome_dir", std::format("{}", methylome_dir)},
        {"index_dir", std::format("{}", index_dir)},
        {"log_filename", std::format("{}", log_filename)},
        {"log_level", std::format("{}", log_level)},
        {"local_mode", std::format("{}", local_mode)},
        {"bin_size", std::format("{}", bin_size)},
        {"methylome_names", std::format("{}", methylome_names)},
        {"write_scores", std::format("{}", write_scores)},
        {"count_covered", std::format("{}", count_covered)},
        {"output_file", std::format("{}", output_file)},
        // clang-format on
      });
  }

  [[nodiscard]] auto
  set_common_opts_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    using po::value;
    po::options_description opts("Command line or config file options");
    opts.add_options()
      // clang-format off
      ("hostname,s", value(&hostname)->default_value(hostname_default),
       "server hostname")
      ("port,p", value(&port)->default_value(port_default, ""), "server port")
      ("methylome-dir,d", value(&methylome_dir)->required(),
       "methylome directory (local mode only)")
      ("index-dir,x", value(&index_dir)->required(),
       "genome index directory")
      ("log-level,v", value(&log_level)->default_value(log_level_default),
       "log level {debug,info,warning,error,critical}")
      ("log-file,l", value(&log_filename)->value_name("[arg]"),
       "log file name (defaults: print to screen)")
      // clang-format on
      ;
    return opts;
  }

  [[nodiscard]] auto
  set_cli_only_opts_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    using po::value;
    po::options_description opts("Command line options");
    opts.add_options()
      // clang-format off
      ("help,h", "print this message and exit")
      ("config-file,c",
       value(&config_file)->default_value(get_default_config_file(), ""),
       "use specified config file")
      ("local", po::bool_switch(&local_mode), "run in local mode")
      ("bin-size,b", value(&bin_size),
       "size of genomic bins in base pairs")
      ("genome,g", value(&genome_name)->required(), "genome name")
      ("methylomes,m", value(&methylome_names)->required(),
       "methylome names (comma separated)")
      ("output,o", value(&output_file)->required(), "output file")
      ("covered", po::bool_switch(&count_covered),
       "count covered sites for each interval")
      ("score", po::bool_switch(&write_scores),
       "output weighted methylation in bedgraph format")
      // clang-format on
      ;
    return opts;
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(bins_argset, (), (
  hostname,
  port,
  methylome_dir,
  index_dir,
  log_filename,
  log_level
)
)
// clang-format on

}  // namespace transferase

[[nodiscard]] static inline auto
split_comma(const auto &s) {
  return s | std::views::split(',') | std::views::transform([](const auto r) {
           return std::string(std::cbegin(r), std::cend(r));
         }) |
         std::ranges::to<std::vector<std::string>>();
}

auto
command_bins_main(int argc,
                  char *argv[])  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  -> int {
  static constexpr auto command = "bins";
  static const auto usage = std::format("Usage: xfrase bins [options]\n");
  static const auto about_msg =
    std::format("xfrase {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  transferase::bins_argset args;
  const auto ecc = args.parse(argc, argv, usage, about_msg, description_msg);
  if (ecc == argument_error_code::help_requested)
    return EXIT_SUCCESS;
  if (ecc)
    return EXIT_FAILURE;

  auto &lgr = transferase::logger::instance(transferase::shared_from_cout(),
                                            command, args.log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  args.log_options();

  std::error_code ec;
  const auto index =
    transferase::genome_index::read(args.index_dir, args.genome_name, ec);
  if (ec) {
    lgr.error("Failed to read genome index {} {}: {}", args.index_dir,
              args.genome_name, ec);
    return EXIT_FAILURE;
  }

  using transferase::request_type_code;
  const auto request_type = args.count_covered ? request_type_code::bins_covered
                                               : request_type_code::bins;

  const auto methylome_names = split_comma(args.methylome_names);
  const auto req = transferase::request{request_type, index.get_hash(),
                                        args.bin_size, methylome_names};

  const auto resource = transferase::methylome_resource{
    .directory = args.local_mode ? args.methylome_dir : std::string{},
    .hostname = args.hostname,
    .port_number = args.port,
  };

  const auto bins_start{std::chrono::high_resolution_clock::now()};

  using transferase::level_element_covered_t;
  using transferase::level_element_t;
  const auto results =
    args.count_covered
      ? resource.get_levels<level_element_covered_t>(req, index, ec)
      : resource.get_levels<level_element_t>(req, index, ec);

  const auto bins_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for bins query: {:.3}s",
            duration(bins_start, bins_stop));

  if (ec) {
    lgr.error("Error: {}", ec);
    return EXIT_FAILURE;
  }

  const auto outmgr = transferase::bins_output_mgr{
    args.output_file, args.bin_size, index, args.write_scores};

  const auto output_start{std::chrono::high_resolution_clock::now()};
  ec = write_output(outmgr, results);
  const auto output_stop{std::chrono::high_resolution_clock::now()};
  // ADS: elapsed time for output will include conversion to scores
  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));
  return EXIT_SUCCESS;
}
