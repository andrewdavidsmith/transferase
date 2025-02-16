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

#include "command_query.hpp"

static constexpr auto about = R"(
query methylation levels in genomic intervals or bins
)";

static constexpr auto description = R"(
The query command accepts either a set of genomic intervals or a bin
size, along with a set of methylome names. It generates a summary of
the methylation levels in each interval/bin, for each methylome. This
command runs in two modes, local and remote. The local mode is for
analyzing data on your local storage: either your own data or data
that you downloaded. The remote mode is for analyzing methylomes in a
remote database on a server. Depending on the mode you select, the
options you must specify will differ.
)";

static constexpr auto examples = R"(
Examples:

xfr query -g hg38 -m methylome_name -o output.bed -i input.bed

xfr query -g hg38 -m methylome_name -o output.bed -i input.bed

xfr query --local -x index_dir -g hg38 -d methylome_dir \
    -m methylome_name -o output.bed -i input.bed

xfr query -x index_dir -g hg38 -s example.com -m SRX012345 \
    -o output.bed -b 5000

xfr query --local -d methylome_dir -x index_dir -g hg38 \
    -m methylome_name -o output.bed -b 1000
)";

#include "bins_writer.hpp"
#include "client_config.hpp"
#include "genome_index.hpp"
#include "genomic_interval.hpp"
#include "intervals_writer.hpp"
#include "level_container.hpp"
#include "level_element.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_interface.hpp"
#include "output_format_type.hpp"
#include "request.hpp"
#include "request_type_code.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <algorithm>  // IWYU pragma: keep
#include <cerrno>     // for errno
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>  // for std::size, for std::cbegin
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // for std::tuple
#include <vector>

[[nodiscard]] static inline auto
format_methylome_names_brief(const std::string &methylome_names)
  -> std::string {
  static constexpr auto max_names_width = 50;
  if (std::size(methylome_names) > max_names_width)
    return methylome_names.substr(0, max_names_width - 3) + "...";
  return methylome_names;
}

[[nodiscard]] inline auto
write_output(const auto &outmgr, const auto &results) -> std::error_code {
  std::error_code ec;
  std::visit([&](const auto &arg) { ec = outmgr.write_output(arg); }, results);
  return ec;
}

[[nodiscard]] inline auto
read_methylomes_file(const std::string &filename,
                     std::error_code &ec) -> std::vector<std::string> {
  std::ifstream in(filename);
  if (!in) {
    ec = std::make_error_code(std::errc(errno));
    return {};
  }

  std::vector<std::string> names;
  std::string line;
  while (getline(in, line))
    names.push_back(line);

  ec = std::error_code{};
  return names;
}

/// Read query intervals, check that they are sorted and valid
[[nodiscard]] static auto
read_intervals(const transferase::genome_index &index,
               const std::string &intervals_file, std::error_code &error)
  -> std::vector<transferase::genomic_interval> {
  auto &lgr = transferase::logger::instance();
  auto intervals =
    transferase::genomic_interval::read(index, intervals_file, error);
  if (error) {
    lgr.error("Error reading intervals file {}: {}", intervals_file, error);
    return {};
  }
  if (!transferase::genomic_interval::are_sorted(intervals)) {
    lgr.error("Intervals not sorted: {}", intervals_file);
    error = std::make_error_code(std::errc::invalid_argument);
    return {};
  }
  if (!transferase::genomic_interval::are_valid(intervals)) {
    lgr.error("Intervals not valid: {} (negative size found)", intervals_file);
    error = std::make_error_code(std::errc::invalid_argument);
    return {};
  }
  lgr.info("Number of intervals: {}", std::size(intervals));
  return intervals;
}

[[nodiscard]] static auto
do_intervals_query(const std::string &intervals_file, const bool count_covered,
                   const transferase::output_format_t out_fmt,
                   const std::uint32_t min_reads,
                   const std::string &output_file,
                   const transferase::genome_index &index,
                   const transferase::methylome_interface &interface,
                   const std::vector<std::string> &methylome_names,
                   const transferase::request_type_code &request_type) {
  auto &lgr = transferase::logger::instance();
  // Read query intervals and validate them
  std::error_code error;
  const auto intervals = read_intervals(index, intervals_file, error);
  if (error)
    return error;  // error message handled already

  // Convert intervals into query
  const auto format_query_start{std::chrono::high_resolution_clock::now()};
  auto query = index.make_query(intervals);
  const auto format_query_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time to prepare query: {:.3}s",
            duration(format_query_start, format_query_stop));

  const auto req = transferase::request{request_type, index.get_hash(),
                                        std::size(intervals), methylome_names};

  const auto query_start{std::chrono::high_resolution_clock::now()};

  using transferase::level_element_covered_t;
  using transferase::level_element_t;
  const auto results =
    count_covered
      ? interface.get_levels<level_element_covered_t>(req, query, error)
      : interface.get_levels<level_element_t>(req, query, error);
  if (error) {
    lgr.error("Error obtaining levels: {}", error);
    return error;
  }
  const auto query_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for query: {:.3}s",
            duration(query_start, query_stop));

  const auto outmgr = transferase::intervals_writer{
    // clang-format off
    output_file,
    index,
    out_fmt,
    methylome_names,
    min_reads,
    intervals,
    // clang-format on
  };
  // outmgr.min_reads = args.min_reads;

  const auto output_start{std::chrono::high_resolution_clock::now()};
  error = write_output(outmgr, results);
  const auto output_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));
  if (error) {
    lgr.error("Error writing output: {}", error);
    return error;
  }
  return std::error_code{};
}

[[nodiscard]] static auto
do_bins_query(const std::uint32_t bin_size, const bool count_covered,
              const transferase::output_format_t out_fmt,
              const std::uint32_t min_reads, const std::string &output_file,
              const transferase::genome_index &index,
              const transferase::methylome_interface &interface,
              const std::vector<std::string> &methylome_names,
              const transferase::request_type_code &request_type) {
  auto &lgr = transferase::logger::instance();
  // Read query intervals and validate them
  const auto req = transferase::request{request_type, index.get_hash(),
                                        bin_size, methylome_names};
  std::error_code error;
  const auto query_start{std::chrono::high_resolution_clock::now()};
  using transferase::level_element_covered_t;
  using transferase::level_element_t;
  const auto results =
    count_covered
      ? interface.get_levels<level_element_covered_t>(req, index, error)
      : interface.get_levels<level_element_t>(req, index, error);
  if (error) {
    lgr.error("Error obtaining levels: {}", error);
    return error;
  }
  const auto query_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for query: {:.3}s",
            duration(query_start, query_stop));

  const auto outmgr = transferase::bins_writer{
    // clang-format off
    output_file,
    index,
    out_fmt,
    methylome_names,
    min_reads,
    bin_size,
    // clang-format on
  };
  // outmgr.min_reads = args.min_reads;

  const auto output_start{std::chrono::high_resolution_clock::now()};
  error = write_output(outmgr, results);
  const auto output_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));
  if (error) {
    lgr.error("Error writing output: {}", error);
    return error;
  }
  return std::error_code{};
}

auto
command_query_main(int argc, char *argv[]) -> int {  // NOLINT
  static constexpr auto command = "query";
  static const auto usage = std::format("Usage: xfr {} [options]\n", command);
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  namespace xfr = transferase;

  // arguments that can be set in the client_config are held there
  xfr::client_config cfg;

  // arguments that determine the type of computation done on the
  // server and the type of data communicated
  std::uint32_t bin_size{};
  std::string intervals_file;
  bool count_covered{false};

  // the two possible sources of names of methylomes to query
  std::string methylome_names;
  std::string methylomes_file;

  // the genome associated with the methylomes -- could come from a
  // lookup in transferase metadata
  std::string genome_name;

  // where to put the output and in what format
  std::string output_file;
  xfr::output_format_t out_fmt{};
  std::uint32_t min_reads{1};  // relevant for dfscores

  // run in local mode
  bool local_mode{};

  // get the default config directory to use as a fallback
  std::error_code default_config_dir_error;
  cfg.config_dir =
    xfr::client_config::get_default_config_dir(default_config_dir_error);

  namespace po = boost::program_options;

  po::options_description opts("Options");
  opts.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("config-dir,c", po::value(&cfg.config_dir), "specify a config directory")
    ("local", po::bool_switch(&local_mode), "run in local mode")
    ("bin-size,b", po::value(&bin_size), "size of genomic bins")
    ("intervals-file,i", po::value(&intervals_file), "intervals file")
    ("genome,g", po::value(&genome_name)->required(), "genome name")
    ("methylomes,m", po::value(&methylome_names),
     "methylome names (comma separated)")
    ("methylomes-file,M", po::value(&methylomes_file),
     "methylomes file (text file; one methylome per line)")
    ("out-file,o", po::value(&output_file)->required(), "output file")
    ("covered,C", po::bool_switch(&count_covered),
     "count covered sites for each interval")
    ("out-fmt,f", po::value(&out_fmt)->default_value(xfr::output_format_t::counts, "1"),
     "output format {counts=1, bedgraph=2, dataframe=3, dfscores=4}")
    ("min-reads,r", po::value(&min_reads)->default_value(1),
     "for fractional output, require this many reads")
    ("hostname,s", po::value(&cfg.hostname), "server hostname")
    ("port,p", po::value(&cfg.port), "server port")
    ("methylome-dir,d", po::value(&cfg.methylome_dir),
     "methylome directory (local mode only)")
    ("index-dir,x", po::value(&cfg.index_dir),
     "genome index directory")
    ("log-level,v", po::value(&cfg.log_level)->default_value(xfr::log_level_t::info),
     "{debug, info, warning, error, critical}")
    ("log-file,l", po::value(&cfg.log_file)->value_name("[arg]"),
     "log file name (defaults: print to screen)")
    // clang-format on
    ;
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, opts), vm);
    if (vm.count("help") || argc == 1) {
      std::println("{}\n{}", about_msg, usage);
      opts.print(std::cout);
      std::println("\n{}", description_msg);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    return EXIT_FAILURE;
  }

  // Attempting to load values from config file in cfg.config_dir but
  // deferring error reporting as all values might have been specified
  // on the command line. If the user didn't specify a config dir,
  // this will try to parse from the default.
  std::error_code error;
  cfg.read_config_file_no_overwrite(error);

  auto &lgr =
    xfr::logger::instance(xfr::shared_from_cout(), command, cfg.log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  // validate relationships between arguments
  if (local_mode && cfg.methylome_dir.empty()) {
    const auto msg = R"(Local mode but methylome dir not specified {}: {})";
    if (default_config_dir_error)
      lgr.debug(msg, "; failed to parse config: {}", default_config_dir_error);
    else
      lgr.debug(msg, "; not found in config: {}", cfg.config_dir);
    return EXIT_FAILURE;
  }
  if (!local_mode && (cfg.hostname.empty() || cfg.port.empty())) {
    const auto msg = R"(Remote mode but hostname or port not specified {} {})";
    if (default_config_dir_error)
      lgr.debug(msg, "; failed to parse config: ", default_config_dir_error);
    else
      lgr.debug(msg, "; not found in config: ", cfg.config_dir);
    return EXIT_FAILURE;
  }

  if (cfg.index_dir.empty()) {
    const auto msg = R"(Index dir not specified)";
    if (default_config_dir_error)
      lgr.debug(msg, "; failed to parse config: ", default_config_dir_error);
    else
      lgr.debug(msg, "; not found in config: ", cfg.config_dir);
    return EXIT_FAILURE;
  }
  if ((bin_size == 0) == intervals_file.empty()) {
    lgr.error("Error: specify exactly one of bins-size or intervals-file");
    return EXIT_FAILURE;
  }
  if (methylome_names.empty() == methylomes_file.empty()) {
    lgr.error("Error: specify exactly one of methylomes or methylomes-file");
    return EXIT_FAILURE;
  }

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Config dir", cfg.config_dir},
    {"Hostname", cfg.hostname},
    {"Port", cfg.port},
    {"Methylome dir", cfg.methylome_dir},
    {"Index dir", cfg.index_dir},
    {"Log file", cfg.log_file},
    {"Log level", std::format("{}", cfg.log_level)},
    {"Bin size", std::format("{}", bin_size)},
    {"Intervals file", intervals_file},
    {"Count covered", std::format("{}", count_covered)},
    {"Methylome names", format_methylome_names_brief(methylome_names)},
    {"Methylomes file", methylomes_file},
    {"Genome name", genome_name},
    {"Output file", output_file},
    {"Output format", std::format("{}", out_fmt)},
    {"Min reads", std::format("{}", min_reads)},
    {"Local mode", std::format("{}", local_mode)},
    // clang-format on
  };
  xfr::log_args<xfr::log_level_t::info>(args_to_log);

  const auto index =
    xfr::genome_index::read(cfg.get_index_dir(), genome_name, error);
  if (error) {
    lgr.error("Failed to read genome index {} {}: {}", cfg.get_index_dir(),
              genome_name, error);
    return EXIT_FAILURE;
  }

  const auto interface = xfr::methylome_interface{
    // directory
    local_mode ? cfg.methylome_dir : std::string{},
    // hostname
    cfg.hostname,
    // port number
    cfg.port,
  };

  // get methylome names either parsed from command line or in a file
  const auto methylomes = !methylomes_file.empty()
                            ? read_methylomes_file(methylomes_file, error)
                            : split_comma(methylome_names);
  if (error) {
    lgr.error("Error reading methylomes file {}: {}", methylomes_file, error);
    return EXIT_FAILURE;
  }

  // validate the methylome names
  const auto invalid_name =
    std::ranges::find_if_not(methylomes, &xfr::methylome::is_valid_name);
  if (invalid_name != std::cend(methylomes)) {
    lgr.error("Error: invalid methylome name \"{}\"", *invalid_name);
    return EXIT_FAILURE;
  }

  const bool intervals_query = (bin_size == 0);
  const auto request_type =
    intervals_query ? (count_covered ? xfr::request_type_code::intervals_covered
                                     : xfr::request_type_code::intervals)
                    : (count_covered ? xfr::request_type_code::bins_covered
                                     : xfr::request_type_code::bins);
  error =
    intervals_query
      ? do_intervals_query(intervals_file, count_covered, out_fmt, min_reads,
                           output_file, index, interface, methylomes,
                           request_type)
      : do_bins_query(bin_size, count_covered, out_fmt, min_reads, output_file,
                      index, interface, methylomes, request_type);
  lgr.info("Completed query with status: {}", error);

  return error ? EXIT_FAILURE : EXIT_SUCCESS;
}
