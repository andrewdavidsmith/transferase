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

xfr query -g hg38 -m methylomes_file.txt -o output.bed -i input.bed

xfr query --local -x index_dir -g hg38 -d methylome_dir \
    -m methylome_name -o output.bed -i input.bed

xfr query -x index_dir -g hg38 -s example.com -m SRX012345 \
    -o output.bed -b 5000

xfr query --local -d methylome_dir -x index_dir -g hg38 \
    -m methylome_name -o output.bed -b 1000
)";

#include "bins_writer.hpp"
#include "cli_common.hpp"
#include "client_config.hpp"
#include "genome_index.hpp"
#include "genomic_interval.hpp"
#include "intervals_writer.hpp"
#include "level_element.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_interface.hpp"
#include "output_format_type.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "request_type_code.hpp"
#include "utilities.hpp"

#include "macos_helper.hpp"

#include "CLI11/CLI11.hpp"

#include <asio.hpp>

#include <algorithm>  // IWYU pragma: keep
#include <cerrno>     // for errno
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>  // for std::size, for std::cbegin
#include <map>
#include <memory>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // for std::tuple
#include <vector>

[[nodiscard]] static inline auto
format_methylome_names_brief(const std::vector<std::string> &names)
  -> std::string {
  static constexpr auto max_names_width = 50;
  // auto joined =
  //   names | std::views::join_with(' ') | std::ranges::to<std::string>();
  auto joined = join_with(names, ' ');
  if (std::size(joined) > max_names_width)
    return std::format("{}...{} ({} methylomes)", names.front(), names.back(),
                       std::size(names));
  return joined;
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
  lgr.debug("Number of intervals in {}: {}", intervals_file,
            std::size(intervals));
  return intervals;
}

template <typename level_element>
[[nodiscard]] static auto
do_intervals_query(const std::string &intervals_file,
                   const transferase::output_format_t out_fmt,
                   const std::uint32_t min_reads,
                   const std::string &output_file,
                   const transferase::genome_index &index,
                   const transferase::methylome_interface &interface,
                   const std::vector<std::string> &methylome_names,
                   const transferase::request_type_code &request_type,
                   const bool write_n_cpgs) {
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

  const auto results = interface.get_levels<level_element>(req, query, error);
  if (error) {
    lgr.error("Error obtaining levels: {}", error);
    return error;
  }
  const auto query_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for query: {:.3}s",
            duration(query_start, query_stop));

  const auto n_cpgs =
    write_n_cpgs ? query.get_n_cpgs() : std::vector<std::uint32_t>{};

  const auto outmgr = transferase::intervals_writer{
    // clang-format off
    output_file,
    index,
    out_fmt,
    methylome_names,
    min_reads,
    n_cpgs,
    intervals,
    // clang-format on
  };

  const auto output_start{std::chrono::high_resolution_clock::now()};
  error = outmgr.write_output(results);
  if (error) {
    lgr.error("Error writing output: {}", error);
    return error;
  }
  const auto output_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));

  return std::error_code{};
}

template <typename level_element>
[[nodiscard]] static auto
do_bins_query(const std::uint32_t bin_size,
              const transferase::output_format_t out_fmt,
              const std::uint32_t min_reads, const std::string &output_file,
              const transferase::genome_index &index,
              const transferase::methylome_interface &interface,
              const std::vector<std::string> &methylome_names,
              const transferase::request_type_code &request_type,
              const bool write_n_cpgs) {
  auto &lgr = transferase::logger::instance();
  // Read query intervals and validate them
  const auto req = transferase::request{request_type, index.get_hash(),
                                        bin_size, methylome_names};
  std::error_code error;
  const auto query_start{std::chrono::high_resolution_clock::now()};
  const auto results = interface.get_levels<level_element>(req, index, error);
  if (error) {
    lgr.error("Error obtaining levels: {}", error);
    return error;
  }
  const auto query_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for query: {:.3}s",
            duration(query_start, query_stop));

  const auto n_cpgs =
    write_n_cpgs ? index.get_n_cpgs(bin_size) : std::vector<std::uint32_t>{};

  const auto outmgr = transferase::bins_writer{
    // clang-format off
    output_file,
    index,
    out_fmt,
    methylome_names,
    min_reads,
    n_cpgs,
    bin_size,
    // clang-format on
  };

  const auto output_start{std::chrono::high_resolution_clock::now()};

  error = outmgr.write_output(results);

  const auto output_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));
  if (error) {
    lgr.error("Error writing output: {}", error);
    return error;
  }
  return std::error_code{};
}

[[nodiscard]] static inline auto
get_methylome_names(const std::vector<std::string> &possibly_methylome_names,
                    std::error_code &error) -> std::vector<std::string> {
  if (possibly_methylome_names.size() > 1)
    return possibly_methylome_names;
  const bool is_file =
    std::filesystem::exists(possibly_methylome_names.front()) &&
    std::filesystem::is_regular_file(possibly_methylome_names.front());
  if (is_file) {
    auto names_from_file =
      read_methylomes_file(possibly_methylome_names.front(), error);
    if (!error)
      return names_from_file;
    return {};
  }
  return possibly_methylome_names;
}

auto
command_query_main(int argc, char *argv[]) -> int {  // NOLINT
  static constexpr auto log_level_default = transferase::log_level_t::info;
  static constexpr auto out_fmt_default = transferase::output_format_t::counts;
  static constexpr auto command = "query";
  static const auto usage = std::format("Usage: xfr {} [options]", command);
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  namespace xfr = transferase;

  // arguments that can be set in the client_config are held there
  xfr::client_config cfg;
  cfg.log_level = log_level_default;

  // arguments that determine the type of computation done on the
  // server and the type of data communicated
  std::uint32_t bin_size{};
  std::string intervals_file;
  bool count_covered{false};
  bool write_n_cpgs{false};

  // the two possible sources of names of methylomes to query
  std::vector<std::string> methylome_names;

  // the genome associated with the methylomes -- could come from a
  // lookup in transferase metadata
  std::string genome_name;

  // where to put the output and in what format
  std::string output_file;
  xfr::output_format_t out_fmt{out_fmt_default};
  std::uint32_t min_reads{0};  // relevant for 'scores' and 'dfscores'

  // run in local mode
  bool local_mode{};

  // get the default config directory to use as a fallback
  std::error_code default_config_dir_error;
  cfg.config_dir =
    xfr::client_config::get_default_config_dir(default_config_dir_error);

  CLI::App app{about_msg};
  argv = app.ensure_utf8(argv);
  app.formatter(std::make_shared<transferase_formatter>());

  app.usage(usage);
  if (argc >= 2)
    app.footer(description_msg);
  app.get_formatter()->label("REQUIRED", "");
  app.set_help_flag("-h,--help", "print a detailed help message and exit");
  // clang-format off
  app.add_option("-c,--config-dir", cfg.config_dir, "specify a config directory")
    ->option_text("DIR")
    ->check(CLI::ExistingDirectory);
  const auto intervals_file_opt =
    app.add_option("-i,--intervals-file", intervals_file,
                   "input query intervals file in BED format")
    ->option_text("FILE")
    ->check(CLI::ExistingFile);
  app.add_option("-b,--bin-size", bin_size, "size of genomic bins to query")
    ->option_text("INT")
    ->excludes(intervals_file_opt);
  app.add_option("-g,--genome", genome_name, "name of the reference genome")
    ->required();
  app.add_option("-m,--methylomes", methylome_names,
                 "one or more methylomes names, or a file "
                 "with one methylome name per line")
    ->required();
  app.add_option("-o,--out-file", output_file, "output file");
  app.add_flag("-C,--covered", count_covered,
               "count covered sites for each query interval");
  app.add_option("-f,--out-fmt", out_fmt,
                 std::format("output format {}", xfr::output_format_help_str))
    ->option_text(std::format("[{}]", out_fmt_default))
    ->transform(CLI::CheckedTransformer(xfr::output_format_cli11, CLI::ignore_case));
  app.add_flag("--n-cpgs", write_n_cpgs,
               "write the number of CpG sites for each query interval "
               "as the final column of the output");
  app.add_option("-r,--min-reads", min_reads,
                 "for scores output, if the score is based on fewer "
                 "than this number of reads, output a value of NA")
    ->option_text("INT");
  const auto hostname_opt =
    app.add_option("-s,--hostname", cfg.hostname, "server hostname")
    ->option_text("TEXT");
  app.add_option("-p,--port", cfg.port, "server port")
    ->option_text("INT")
    ->needs(hostname_opt);
  const auto local_mode_opt =
    app.add_flag("-L,--local", local_mode, "run in local mode")
    ->option_text(" ")
    ->excludes(hostname_opt);
  app.add_option("-d,--methylome-dir", cfg.methylome_dir,
                 "methylome directory for local mode")
    ->option_text("DIR")
    ->needs(local_mode_opt)
    ->check(CLI::ExistingDirectory);
  app.add_option("-x,--index-dir", cfg.index_dir,
                 "genome index directory")
    ->option_text("DIR")
    ->check(CLI::ExistingDirectory);
  app.add_option("-v,--log-level", cfg.log_level,
                 std::format("log level {}", xfr::log_level_help_str))
    ->option_text(std::format("[{}]", log_level_default))
    ->transform(CLI::CheckedTransformer(xfr::str_to_level, CLI::ignore_case));
  // clang-format on

  if (argc < 2) {
    std::println("{}", app.help());
    return EXIT_SUCCESS;
  }
  CLI11_PARSE(app, argc, argv);

  // make any assigned paths absolute so that subsequent composition
  // with any config_dir will not overwrite any relative path
  // specified on the command line.
  cfg.make_paths_absolute();

  // Attempting to load values from config file in cfg.config_dir but
  // deferring error reporting as all values might have been specified
  // on the command line. If the user didn't specify a config dir,
  // this will try to parse from the default.
  std::error_code read_config_file_error;
  cfg.read_config_file_no_overwrite(read_config_file_error);

  auto &lgr =
    xfr::logger::instance(xfr::shared_from_cout(), command, cfg.log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  // validate that required data is provided somehow and report the problem
  // otherwise
  if (local_mode && cfg.methylome_dir.empty()) {
    const auto msg = "Local mode but methylome dir not specified.";
    if (read_config_file_error)
      lgr.error("{} Failed to read config: {} ({})", msg, cfg.config_dir,
                read_config_file_error);
    else if (default_config_dir_error)
      lgr.error("{} Failed to read default config ({})", msg,
                default_config_dir_error);
    else
      lgr.error("{} Not found in: {}", msg, cfg.config_dir);
    return EXIT_FAILURE;
  }

  if (!local_mode && (cfg.hostname.empty() || cfg.port.empty())) {
    const auto msg = std::format(R"(Remote mode but hostname={} and port={}.)",
                                 cfg.hostname, cfg.port);
    if (read_config_file_error)
      lgr.error("{} Failed to read config: {} ({})", msg, cfg.config_dir,
                read_config_file_error);
    else if (default_config_dir_error)
      lgr.error("{} Failed to parse default config: {}", msg,
                default_config_dir_error);
    else
      lgr.error("{} Not found in config: {}", msg, cfg.config_dir);
    return EXIT_FAILURE;
  }

  if (cfg.index_dir.empty()) {
    const auto msg = "Index dir not specified";
    if (read_config_file_error)
      lgr.error("{} failed to parse config: {} ({})", msg, cfg.config_dir,
                read_config_file_error);
    else if (default_config_dir_error)
      lgr.error("{} failed to parse config: ", msg, default_config_dir_error);
    else
      lgr.error("{} not found in config: ", msg, cfg.config_dir);
    return EXIT_FAILURE;
  }

  if ((bin_size == 0) == intervals_file.empty()) {
    lgr.error("Error: specify exactly one of bins-size or intervals-file");
    return EXIT_FAILURE;
  }

  std::error_code error;
  const auto index =
    xfr::genome_index::read(cfg.get_index_dir(), genome_name, error);
  if (error) {
    lgr.error("Failed to read genome index {} {}: {}", cfg.get_index_dir(),
              genome_name, error);
    return EXIT_FAILURE;
  }

  const auto interface = xfr::methylome_interface{
    cfg.methylome_dir,
    cfg.hostname,
    cfg.port,
    local_mode,
  };

  // get methylome names either parsed from command line or in a file
  const auto methylomes = get_methylome_names(methylome_names, error);
  if (error) {
    lgr.error("Error identifying methylomes from {}: {}",
              format_methylome_names_brief(methylome_names), error);
    return EXIT_FAILURE;
  }

  // validate the methylome names
  const auto invalid_name =
    std::ranges::find_if_not(methylomes, &xfr::methylome::is_valid_name);
  if (invalid_name != std::cend(methylomes)) {
    lgr.error("Error: invalid methylome name \"{}\"", *invalid_name);
    return EXIT_FAILURE;
  }

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Config dir", cfg.config_dir},
    {"Hostname", cfg.hostname},
    {"Port", cfg.port},
    {"Methylome dir", cfg.methylome_dir},
    {"Index dir", cfg.index_dir},
    {"Log level", std::format("{}", cfg.log_level)},
    {"Bin size", std::format("{}", bin_size)},
    {"Intervals file", intervals_file},
    {"Count covered", std::format("{}", count_covered)},
    {"Methylome names", format_methylome_names_brief(methylomes)},
    {"Genome name", genome_name},
    {"Output file", output_file},
    {"Output format", std::format("{}", out_fmt)},
    {"Min reads", std::format("{}", min_reads)},
    {"Local mode", std::format("{}", local_mode)},
    // clang-format on
  };
  xfr::log_args<xfr::log_level_t::debug>(args_to_log);

  const bool intervals_query = (bin_size == 0);
  const auto request_type =
    intervals_query ? (count_covered ? xfr::request_type_code::intervals_covered
                                     : xfr::request_type_code::intervals)
                    : (count_covered ? xfr::request_type_code::bins_covered
                                     : xfr::request_type_code::bins);

  // clang-format off
  error =
    intervals_query
    ? (count_covered ? do_intervals_query<xfr::level_element_covered_t>(
                         intervals_file, out_fmt, min_reads, output_file, index,
                         interface, methylomes, request_type, write_n_cpgs)
                     : do_intervals_query<xfr::level_element_t>(
                         intervals_file, out_fmt, min_reads, output_file, index,
                         interface, methylomes, request_type, write_n_cpgs))
    : (count_covered ? do_bins_query<xfr::level_element_covered_t>(
                         bin_size, out_fmt, min_reads, output_file, index,
                         interface, methylomes, request_type, write_n_cpgs)
                     : do_bins_query<xfr::level_element_t>(
                         bin_size, out_fmt, min_reads, output_file, index,
                         interface, methylomes, request_type, write_n_cpgs));
  // clang-format on

  // ADS: below is because error code '0' is printed as "Undefined
  // error" on macos.
  if (error) {
    lgr.critical("Failed to complete query: {}", error);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
