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

xfr query -s example.com -x index_dir -g hg38 -m methylome_name \
    -o output.bed -i input.bed

xfr query -c config_file.toml -g hg38 -m methylome_name \
    -o output.bed -i input.bed

xfr query --local -x index_dir -g hg38 -d methylome_dir \
    -m methylome_name -o output.bed -i input.bed

xfr query -x index_dir -g hg38 -s example.com -m SRX012345 \
    -o output.bed -b 5000

xfr query --local -d methylome_dir -x index_dir -g hg38 \
    -m methylome_name -o output.bed -b 1000
)";

#include "arguments.hpp"
#include "bins_writer.hpp"
#include "genome_index.hpp"
#include "genomic_interval.hpp"
#include "intervals_writer.hpp"
#include "level_element.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_interface.hpp"
#include "output_format_type.hpp"
#include "request.hpp"
#include "request_type_code.hpp"
#include "utilities.hpp"

#include <boost/describe.hpp>
#include <boost/program_options.hpp>

#include <algorithm>  // IWYU pragma: keep
#include <cerrno>     // for errno
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
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

namespace transferase {

struct query_argset : argset_base<query_argset> {
  static constexpr auto default_config_filename =
    "transferase_client_config.toml";

  static auto
  get_default_config_file_impl() -> std::string {
    std::error_code ec;
    const auto config_dir = get_config_dir_default(ec);
    if (ec)
      return {};
    return std::filesystem::path{config_dir} / default_config_filename;
  }

  static constexpr auto log_level_default{log_level_t::info};
  static constexpr auto out_fmt_default{output_format_t::counts};
  std::string hostname;
  std::string port;
  std::string methylome_dir;
  std::string index_dir;
  std::string log_filename;
  log_level_t log_level{};

  bool local_mode{};
  std::uint32_t bin_size{};
  std::string intervals_file{};
  std::string methylome_names{};
  std::string methylomes_file{};
  std::string genome_name{};
  std::string labels_dir{};
  output_format_t out_fmt{};
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
        {"methylome_names", format_methylome_names_brief(methylome_names)},
        {"intervals_file", std::format("{}", intervals_file)},
        {"out_fmt", std::format("{}", out_fmt)},
        {"count_covered", std::format("{}", count_covered)},
        {"output_file", std::format("{}", output_file)},
        // clang-format on
      });
  }

  [[nodiscard]] auto
  set_hidden_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    using po::value;
    po::options_description opts;
    opts.add_options()
      // clang-format off
      ("labels-dir", po::value(&labels_dir), "none")
      // clang-format on
      ;
    return opts;
  }

  [[nodiscard]] auto
  set_opts_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    using po::value;
    po::options_description opts("Options");
    opts.add_options()
      // clang-format off
      ("help,h", "print this message and exit")
      ("config-file,c",
       po::value(&config_file)->default_value(get_default_config_file(), ""),
       "use specified config file")
      ("local", po::bool_switch(&local_mode), "run in local mode")
      ("bin-size,b", po::value(&bin_size), "size of genomic bins")
      ("intervals-file,i", po::value(&intervals_file), "intervals file")
      ("genome,g", po::value(&genome_name)->required(), "genome name")
      ("methylomes,m", po::value(&methylome_names),
       "methylome names (comma separated)")
      ("methylomes-file,M", po::value(&methylomes_file),
       "methylomes file (text file; one methylome per line)")
      ("out-file,o", po::value(&output_file)->required(), "output file")
      ("covered", po::bool_switch(&count_covered),
       "count covered sites for each interval")
      ("out-fmt,f", po::value(&out_fmt)->default_value(out_fmt_default),
       "output format {counts=1, bedgraph=2, dataframe=3, dfscores=4}")
      ("hostname,s", po::value(&hostname), "server hostname")
      ("port,p", po::value(&port), "server port")
      ("methylome-dir,d", po::value(&methylome_dir),
       "methylome directory (local mode only)")
      ("index-dir,x", po::value(&index_dir),
       "genome index directory")
      ("log-level,v", po::value(&log_level)->default_value(log_level_default),
       "{debug, info, warning, error, critical}")
      ("log-file,l", po::value(&log_filename)->value_name("[arg]"),
       "log file name (defaults: print to screen)")
      // clang-format on
      ;
    return opts;
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(query_argset, (), (
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
do_intervals_query(const auto &args, const transferase::genome_index &index,
                   const transferase::methylome_interface &interface,
                   const std::vector<std::string> &methylome_names,
                   const transferase::request_type_code &request_type) {
  auto &lgr = transferase::logger::instance();
  // Read query intervals and validate them
  std::error_code error;
  const auto intervals = read_intervals(index, args.intervals_file, error);
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
    args.count_covered
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
    args.output_file,
    index,
    args.out_fmt,
    methylome_names,
    intervals,
    // clang-format on
  };

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
do_bins_query(const auto &args, const transferase::genome_index &index,
              const transferase::methylome_interface &interface,
              const std::vector<std::string> &methylome_names,
              const transferase::request_type_code &request_type) {
  auto &lgr = transferase::logger::instance();
  // Read query intervals and validate them
  const auto req = transferase::request{request_type, index.get_hash(),
                                        args.bin_size, methylome_names};
  std::error_code error;
  const auto query_start{std::chrono::high_resolution_clock::now()};
  using transferase::level_element_covered_t;
  using transferase::level_element_t;
  const auto results =
    args.count_covered
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
    args.output_file,
    index,
    args.out_fmt,
    methylome_names,
    args.bin_size,
    // clang-format on
  };

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

  transferase::query_argset args;
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

  // validate relationships between arguments
  if (args.local_mode && args.methylome_dir.empty()) {
    lgr.error("Error: local mode requires a methylomes directory");
    return EXIT_FAILURE;
  }
  if (args.index_dir.empty()) {
    lgr.error(
      "Error: specify index directory on command line or in config file");
    return EXIT_FAILURE;
  }
  if ((args.bin_size == 0) == args.intervals_file.empty()) {
    lgr.error("Error: specify exactly one of bins-size or intervals-file");
    return EXIT_FAILURE;
  }
  if (args.methylome_names.empty() == args.methylomes_file.empty()) {
    lgr.error("Error: specify exactly one of methylomes or methylomes-file");
    return EXIT_FAILURE;
  }

  std::error_code ec;
  const auto index =
    transferase::genome_index::read(args.index_dir, args.genome_name, ec);
  if (ec) {
    lgr.error("Failed to read genome index {} {}: {}", args.index_dir,
              args.genome_name, ec);
    return EXIT_FAILURE;
  }

  const auto interface = transferase::methylome_interface{
    .directory = args.local_mode ? args.methylome_dir : std::string{},
    .hostname = args.hostname,
    .port_number = args.port,
  };

  // get methylome names either parsed from command line or in a file
  const auto methylome_names =
    !args.methylomes_file.empty()
      ? read_methylomes_file(args.methylomes_file, ec)
      : split_comma(args.methylome_names);
  if (ec) {
    lgr.error("Error reading methylomes file {}: {}", args.methylomes_file, ec);
    return EXIT_FAILURE;
  }

  // validate the methylome names
  const auto invalid_name = std::ranges::find_if_not(
    methylome_names, &transferase::methylome::is_valid_name);
  if (invalid_name != std::cend(methylome_names)) {
    lgr.error("Error: invalid methylome name \"{}\"", *invalid_name);
    return EXIT_FAILURE;
  }

  using transferase::level_element_covered_t;
  using transferase::level_element_t;
  using transferase::request_type_code;

  const bool intervals_query = (args.bin_size == 0);
  const auto request_type =
    intervals_query ? (args.count_covered ? request_type_code::intervals_covered
                                          : request_type_code::intervals)
                    : (args.count_covered ? request_type_code::bins_covered
                                          : request_type_code::bins);
  const auto error =
    intervals_query
      ? do_intervals_query(args, index, interface, methylome_names,
                           request_type)
      : do_bins_query(args, index, interface, methylome_names, request_type);

  return error ? EXIT_FAILURE : EXIT_SUCCESS;
}
