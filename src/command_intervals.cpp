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

#include "command_intervals.hpp"

static constexpr auto about = R"(
summarize methylation levels in each of a set of genomic intervals
)";

static constexpr auto description = R"(
The intervals command accepts a set of genomic intervals and a
methylome, and it generates a summary of the methylation levels in
each interval. This command runs in two modes, local and remote. The
local mode is for analyzing data on your local storage: either your
own data or data that you downloaded. The remote mode is for analyzing
methylomes in a remote database on a server. Depending on the mode you
select, the options you must specify will differ.
)";

static constexpr auto examples = R"(
Examples:

xfrase intervals -s example.com -x index_dir -g hg38 -m methylome_name \
    -o output.bed -i input.bed

xfrase intervals -c config_file.toml -g hg38 -m methylome_name \
    -o output.bed -i input.bed

xfrase intervals --local -x index_dir -g hg38 -d methylome_dir \
    -m methylome_name -o output.bed -i input.bed
)";

#include "arguments.hpp"
#include "genome_index.hpp"
#include "genomic_interval.hpp"
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
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <format>
#include <iterator>  // for std::size
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>
#include <vector>

namespace transferase {

struct intervals_argset : argset_base<intervals_argset> {
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
  std::string intervals_file{};
  std::string methylome_name{};
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
        {"methylome_name", std::format("{}", methylome_name)},
        {"intervals_file", std::format("{}", intervals_file)},
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
    boost::program_options::options_description opts("Command line options");
    // clang-format off
    opts.add_options()
      ("help,h", "print this message and exit")
      ("config-file,c",
       boost::program_options::value(&config_file)
       ->default_value(get_default_config_file(), ""),
       "use specified config file")
      ("local", boost::program_options::bool_switch(&local_mode),
       "run in local mode")
      ("intervals,i", boost::program_options::value(&intervals_file)->required(),
       "intervals file")
      ("genome,g", boost::program_options::value(&genome_name)->required(),
       "genome name")
      ("methylome,m", boost::program_options::value(&methylome_name)->required(),
       "methylome name")
      ("output,o", boost::program_options::value(&output_file)->required(),
       "output file")
      ("covered", boost::program_options::bool_switch(&count_covered),
       "count covered sites for each interval")
      ("score", boost::program_options::bool_switch(&write_scores),
       "output weighted methylation in bedgraph format")
      ;
    // clang-format on
    return opts;
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(intervals_argset, (), (
  hostname,
  port,
  methylome_dir,
  index_dir,
  log_filename,
  log_level,
)
)
// clang-format on

}  // namespace transferase

auto
command_intervals_main(
  int argc, char *argv[])  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  -> int {
  static constexpr auto command = "intervals";
  static const auto usage = std::format("Usage: xfrase intervals [options]\n");
  static const auto about_msg =
    std::format("xfrase {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  transferase::intervals_argset args;
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

  // Read query intervals and validate them
  const auto intervals =
    transferase::genomic_interval::read(index, args.intervals_file, ec);
  if (ec) {
    lgr.error("Error reading intervals file: {} ({})", args.intervals_file, ec);
    return EXIT_FAILURE;
  }
  if (!transferase::genomic_interval::are_sorted(intervals)) {
    lgr.error("Intervals not sorted: {}", args.intervals_file);
    return EXIT_FAILURE;
  }
  if (!transferase::genomic_interval::are_valid(intervals)) {
    lgr.error("Intervals not valid: {} (negative size found)",
              args.intervals_file);
    return EXIT_FAILURE;
  }
  lgr.info("Number of intervals: {}", std::size(intervals));

  // Convert intervals into query
  const auto format_query_start{std::chrono::high_resolution_clock::now()};
  auto query = index.make_query(intervals);
  const auto format_query_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time to prepare query: {:.3}s",
            duration(format_query_start, format_query_stop));

  using transferase::request_type_code;
  const auto request_type = args.count_covered
                              ? request_type_code::intervals_covered
                              : request_type_code::intervals;

  const auto req = transferase::request{args.methylome_name, request_type,
                                        index.get_hash(), std::size(intervals)};

  const auto resource = transferase::methylome_resource{
    .directory = args.local_mode ? args.methylome_dir : std::string{},
    .hostname = args.hostname,
    .port_number = args.port,
  };

  const auto intervals_start{std::chrono::high_resolution_clock::now()};

  using transferase::level_element_covered_t;
  using transferase::level_element_t;
  const auto results =
    args.count_covered
      ? resource.get_levels<level_element_covered_t>(req, query, ec)
      : resource.get_levels<level_element_t>(req, query, ec);
  if (ec) {
    lgr.error("Error obtaining levels: {}", ec);
    return EXIT_FAILURE;
  }

  const auto intervals_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for query: {:.3}s",
            duration(intervals_start, intervals_stop));

  const auto outmgr = transferase::intervals_output_mgr{
    args.output_file, intervals, index, args.write_scores};

  const auto output_start{std::chrono::high_resolution_clock::now()};
  ec = write_output(outmgr, results);
  const auto output_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));
  if (ec) {
    lgr.error("Error writing output: {}", ec);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
