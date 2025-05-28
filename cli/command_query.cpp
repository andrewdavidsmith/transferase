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
query methylation levels in genomic intervals
)";

static constexpr auto description = R"(
The central command in transferase.  The input has two parts:

- A BED format file of genomic intervals or a bin size.
- Methylome names specified directly or in a file.

The output format is highly customizable.  A server should be configured,
either in the default location or a specified directory. Alternatively, all
server information can be specified.  A local most exists, and does not use
any network communication, but even if all data is on the same machine, local
mode is only advantageous in special situations.
)";

static constexpr auto examples = R"(
Examples:

xfr query -g hg38 -o output.txt -i intervals.bed -m SRX081761

xfr query -g hg38 -o output.txt -i intervals.bed -m SRX081761 \
    --bed --scores --verbose

xfr query -g hg38 -o output.txt -i intervals.bed -m methylomes.txt

xfr query -g hg38 -o output.txt -i intervals.bed -m methylomes.json

xfr query --local -x index_dir -d methylome_dir \
    -g hg38 -i intervals.bed -o output.txt -m methylomes.txt

xfr query -g hg38 -o output.txt -b 100000 -m SRX081761

xfr query -g panTro6 -o output.txt -i chimp_intervals.bed -m SRX081763

xfr query -c private_server_config \
    -g hg38 -o output.txt -i intervals.bed -m private_methylomes.txt

xfr query -s localhost -p 5000 -x index_dir \
    -g hg38 -o output.txt -i intervals.bed -m methylomes.txt
)";

#include "bins_writer.hpp"
#include "cli_common.hpp"
#include "client_config.hpp"
#include "genome_index.hpp"
#include "genomic_interval.hpp"
#include "intervals_writer.hpp"
#include "level_element.hpp"
#include "logger.hpp"
#include "macos_helper.hpp"
#include "methylome.hpp"
#include "methylome_interface.hpp"
#include "output_format_type.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "request_type_code.hpp"
#include "utilities.hpp"

#include "CLI11/CLI11.hpp"
#include "nlohmann/json.hpp"

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
#include <type_traits>  // for std::is_same_v
#include <utility>      // for std::pair
#include <variant>      // for std::tuple
#include <vector>

namespace xfr = transferase;

/// Gather options related to output
struct output_options {
  std::string outfile;
  xfr::output_format_t outfmt{};
  std::uint32_t min_reads{};
  bool write_n_cpgs{};
};

[[nodiscard]] static inline auto
format_methylome_names_brief(const std::vector<std::string> &names)
  -> std::string {
  static constexpr auto max_names_width = 50;
  // auto joined =
  //   names | std::views::join_with(' ') | std::ranges::to<std::string>();
  auto joined = join_with(names, ' ');
  if (std::size(joined) > max_names_width)
    return joined.substr(0, max_names_width) + std::string("...");
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
read_intervals(const xfr::genome_index &index,
               const std::string &intervals_file,
               std::error_code &error) -> std::vector<xfr::genomic_interval> {
  auto &lgr = xfr::logger::instance();
  auto intervals = xfr::genomic_interval::read(index, intervals_file, error);
  if (error) {
    lgr.error("Error reading intervals file {}: {}", intervals_file, error);
    return {};
  }
  if (!xfr::genomic_interval::are_sorted(intervals)) {
    lgr.error("Intervals not sorted: {}", intervals_file);
    error = std::make_error_code(std::errc::invalid_argument);
    return {};
  }
  if (!xfr::genomic_interval::are_valid(intervals)) {
    lgr.error("Intervals not valid: {} (negative size found)", intervals_file);
    error = std::make_error_code(std::errc::invalid_argument);
    return {};
  }
  lgr.debug("Number of intervals in {}: {}", intervals_file,
            std::size(intervals));
  return intervals;
}

template <typename level_element = xfr::level_element_t>
[[nodiscard]] static auto
query_intervals(const std::string &intervals_file,
                const output_options &outopts, const xfr::genome_index &index,
                const xfr::methylome_interface &interface,
                const std::vector<std::string> &methylome_names,
                const std::vector<std::string> &alt_names) {
  auto request_type = xfr::request_type_code::intervals;
  if constexpr (std::is_same_v<level_element, xfr::level_element_covered_t>)
    request_type = xfr::request_type_code::intervals_covered;

  auto &lgr = xfr::logger::instance();
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

  const auto req = xfr::request{request_type, index.get_hash(),
                                std::size(intervals), methylome_names};

  const auto query_start{std::chrono::high_resolution_clock::now()};

  const auto results = interface.get_levels<level_element>(req, query, error);
  if (error) {
    lgr.debug("Error obtaining levels: {}", error);
    return error;
  }
  const auto query_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for query: {:.3}s",
            duration(query_start, query_stop));

  const auto n_cpgs =
    outopts.write_n_cpgs ? query.get_n_cpgs() : std::vector<std::uint32_t>{};

  const auto outmgr = xfr::intervals_writer{
    // clang-format off
    outopts.outfile,
    index,
    outopts.outfmt,
    alt_names,
    outopts.min_reads,
    n_cpgs,
    intervals,
    // clang-format on
  };

  const auto out_start{std::chrono::high_resolution_clock::now()};
  error = outmgr.write_output(results);
  if (error) {
    lgr.error("Error writing output: {}", error);
    return error;
  }
  const auto out_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for output: {:.3}s", duration(out_start, out_stop));

  return std::error_code{};
}

const auto query_intervals_cov = &query_intervals<xfr::level_element_covered_t>;

template <typename level_element = xfr::level_element_t>
[[nodiscard]] static auto
query_bins(const std::uint32_t bin_size, const output_options &outopts,
           const xfr::genome_index &index,
           const xfr::methylome_interface &interface,
           const std::vector<std::string> &methylome_names,
           const std::vector<std::string> &alt_names) {
  auto request_type = xfr::request_type_code::bins;
  if constexpr (std::is_same_v<level_element, xfr::level_element_covered_t>)
    request_type = xfr::request_type_code::bins_covered;

  auto &lgr = xfr::logger::instance();
  // Read query intervals and validate them
  const auto req =
    xfr::request{request_type, index.get_hash(), bin_size, methylome_names};
  std::error_code error;
  const auto query_start{std::chrono::high_resolution_clock::now()};
  const auto results = interface.get_levels<level_element>(req, index, error);
  if (error) {
    lgr.debug("Error obtaining levels: {}", error);
    return error;
  }
  const auto query_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for query: {:.3}s",
            duration(query_start, query_stop));

  const auto n_cpgs = outopts.write_n_cpgs ? index.get_n_cpgs(bin_size)
                                           : std::vector<std::uint32_t>{};

  const auto outmgr = xfr::bins_writer{
    // clang-format off
    outopts.outfile,
    index,
    outopts.outfmt,
    alt_names,
    outopts.min_reads,
    n_cpgs,
    bin_size,
    // clang-format on
  };

  const auto out_start{std::chrono::high_resolution_clock::now()};
  error = outmgr.write_output(results);
  if (error) {
    lgr.error("Error writing output: {}", error);
    return error;
  }
  const auto out_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for output: {:.3}s", duration(out_start, out_stop));

  return std::error_code{};
}

const auto query_bins_cov = &query_bins<xfr::level_element_covered_t>;

[[nodiscard]] static inline auto
read_methylomes_json(const std::string &json_filename, std::error_code &ec)
  -> std::tuple<std::vector<std::string>, std::vector<std::string>> {
  std::ifstream in(json_filename);
  if (!in) {
    ec = std::make_error_code(std::errc(errno));
    return {};
  }
  const nlohmann::json data = nlohmann::json::parse(in, nullptr, false);
  if (data.is_discarded()) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return {{}, {}};
  }
  std::map<std::string, std::string> altname_name;
  try {
    altname_name = data;
  }
  catch (const nlohmann::json::exception &_) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return {{}, {}};
  }

  std::vector<std::pair<std::string, std::string>> sorted_by_alt;
  std::ranges::copy(altname_name, std::back_inserter(sorted_by_alt));

  std::ranges::sort(sorted_by_alt);

  std::vector<std::string> names;
  std::vector<std::string> alt_names;
  for (const auto &[alt_name, name] : sorted_by_alt) {
    names.push_back(name);
    alt_names.push_back(alt_name);
  }
  return {names, alt_names};
}

[[nodiscard]] static inline auto
get_methylome_names(const std::vector<std::string> &possibly_methylome_names,
                    std::error_code &error)
  -> std::tuple<std::vector<std::string>, std::vector<std::string>> {
  if (possibly_methylome_names.size() > 1)
    return {possibly_methylome_names, possibly_methylome_names};
  const bool is_file =
    std::filesystem::exists(possibly_methylome_names.front()) &&
    std::filesystem::is_regular_file(possibly_methylome_names.front());
  if (is_file) {
    // attept to read as JSON with pairs of {name,label}
    const auto [names_from_json, alt_names_from_json] =
      read_methylomes_json(possibly_methylome_names.front(), error);
    if (!error)
      return {names_from_json, alt_names_from_json};
    // JSON didn't work, try just one name per line
    const auto names_from_file =
      read_methylomes_file(possibly_methylome_names.front(), error);
    if (!error)
      return {names_from_file, names_from_file};
    return {{}, {}};
  }
  return {possibly_methylome_names, possibly_methylome_names};
}

auto
command_query_main(int argc, char *argv[]) -> int {  // NOLINT
  static constexpr auto command = "query";
  static const auto usage = std::format("Usage: xfr {} [options]", command);
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  xfr::client_config cfg;

  // arguments that determine what the server computes
  std::uint32_t bin_size{};
  std::string intervals_file;
  bool count_covered{false};

  // the two possible sources of names of methylomes to query
  std::vector<std::string> methylome_names;

  // requiring user to specify assumed genome for safety
  std::string genome_name;

  bool local_mode{false};

  bool verbose{false};
  bool quiet{false};

  // options related to output
  output_options outopts{};
  bool outfmt_scores{false};
  bool outfmt_classic{false};
  bool outfmt_counts{true};
  bool outfmt_bed{false};
  bool outfmt_dataframe{true};

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
  app.get_formatter()->label("REQUIRED", " ");

  app.set_help_flag("-h,--help", "print a detailed help message and exit");
  app
    .add_option("-c,--config-dir", cfg.config_dir, "specify a config directory")
    ->option_text("DIR")
    ->check(CLI::ExistingDirectory);
  const auto intervals_file_opt =
    app
      .add_option("-i,--intervals", intervals_file,
                  "input query intervals file in BED format")
      ->option_text("FILE")
      ->check(CLI::ExistingFile);
  app.add_option("-b,--bin-size", bin_size, "size of genomic bins to query")
    ->option_text("INT")
    ->excludes(intervals_file_opt);
  // ADS: Genome will feel a bit redundant to users. Moving forward, if a
  // query consists of intervals and a methylome, if those are inconsistent
  // (e.g., specifying 'chr22' and asking for a mouse methylome), the "tie
  // breaker" of having the user specify the genome will allow for more
  // informative error reporting.
  app.add_option("-g,--genome", genome_name, "name of the reference genome")
    ->required();
  app
    .add_option("-m,--methylomes", methylome_names,
                "names of methylomes or a file with names")
    ->required();
  const auto hostname_opt = app
                              .add_option("-s,--hostname", cfg.hostname,
                                          "server hostname or IP address")
                              ->option_text("TEXT");
  app.add_option("-p,--port", cfg.port, "server port")
    ->option_text("INT")
    ->needs(hostname_opt);
  const auto local_mode_opt =
    app.add_flag("-L,--local", local_mode, "run in local mode")
      ->option_text(" ")
      ->excludes(hostname_opt);
  app
    .add_option("-d,--methylome-dir", cfg.methylome_dir,
                "methylome directory to use in local mode")
    ->option_text("DIR")
    ->needs(local_mode_opt)
    ->check(CLI::ExistingDirectory);
  app.add_option("-x,--index-dir", cfg.index_dir, "genome index directory")
    ->option_text("DIR")
    ->check(CLI::ExistingDirectory);

  app
    .add_option("-o,--output", outopts.outfile,
                "output filename (directory must exist)")
    ->option_text("FILE")
    ->required();
  const auto scores_opt =
    app.add_flag("--scores", outfmt_scores, "scores output format")
      ->option_text(" ");
  const auto classic_opt =
    app.add_flag("--classic", outfmt_classic, "classic output format")
      ->excludes(scores_opt)
      ->option_text(" ");
  app.add_flag("--counts", outfmt_counts, "counts output format (default)")
    ->excludes(scores_opt)
    ->excludes(classic_opt)
    ->option_text(" ");
  app.add_flag("--covered", count_covered,
               "count covered sites for each reported level");
  app
    .add_flag("--bed", outfmt_bed,
              "no header and first three output columns are BED")
    ->option_text(" ");
  app
    .add_flag("--dataframe", outfmt_dataframe,
              "output has row and column names")
    ->option_text(" ");
  app.add_flag("--cpgs", outopts.write_n_cpgs,
               "report the number of CpGs in each query interval");
  app
    .add_option("-r,--reads", outopts.min_reads,
                "minimum reads below which a score is set to NA")
    ->needs(scores_opt)
    ->option_text("INT");

  const auto verbose_opt = app
                             .add_flag("-v,--verbose", verbose,
                                       "report debug information while running")
                             ->option_text(" ");
  app.add_flag("-q,--quiet", quiet, "only report errors while running")
    ->excludes(verbose_opt)
    ->option_text(" ");

  if (argc < 2) {
    std::println("{}", app.help());
    return EXIT_SUCCESS;
  }
  CLI11_PARSE(app, argc, argv);

  // set the log level based on user options
  cfg.log_level = [&] {
    if (verbose)
      return xfr::log_level_t::debug;
    else if (quiet)
      return xfr::log_level_t::error;
    else
      return xfr::log_level_t::info;
  }();

  // set the output format based on combination of user options
  outopts.outfmt = [&] {
    if (outfmt_bed) {
      if (outfmt_scores)
        return xfr::output_format_t::scores;
      else if (outfmt_classic)
        return xfr::output_format_t::classic;
      else  // if (outfmt_counts)
        return xfr::output_format_t::counts;
    }
    else /*if (outfmt_dataframe) */ {
      if (outfmt_scores)
        return xfr::output_format_t::dfscores;
      else if (outfmt_classic)
        return xfr::output_format_t::dfclassic;
      else  // if (outfmt_counts)
        return xfr::output_format_t::dfcounts;
    }
  }();

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
    lgr.error(
      "Failed to load index for genome {} [index directory: {}][error: {}]",
      genome_name, cfg.get_index_dir(), error);
    lgr.error("Please verify that {} is correct and has been configured",
              genome_name);
    return EXIT_FAILURE;
  }

  const auto interface = xfr::methylome_interface{
    cfg.methylome_dir,
    cfg.hostname,
    cfg.port,
    local_mode,
  };

  // get methylome names either parsed from command line or in a file
  const auto [methylomes, alt_names] =
    get_methylome_names(methylome_names, error);
  if (error) {
    lgr.error("Error identifying methylomes from {}: {}",
              format_methylome_names_brief(methylome_names), error);
    return EXIT_FAILURE;
  }

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Config dir", cfg.config_dir},
    {"Server", cfg.hostname},
    {"Port", cfg.port},
    {"Methylome dir", cfg.methylome_dir},
    {"Index dir", cfg.index_dir},
    {"Log level", std::format("{}", cfg.log_level)},
    {"Bin size", std::format("{}", bin_size)},
    {"Intervals file", intervals_file},
    {"Count covered", std::format("{}", count_covered)},
    {"Number of methylomes", std::format("{}", std::size(methylomes))},
    {"Methylome names", format_methylome_names_brief(methylomes)},
    {"Methylome labels", format_methylome_names_brief(alt_names)},
    {"Genome name", genome_name},
    {"Output file", outopts.outfile},
    {"Output format", std::format("{}", outopts.outfmt)},
    {"Min reads", std::format("{}", outopts.min_reads)},
    {"Local mode", std::format("{}", local_mode)},
    // clang-format on
  };
  xfr::log_args<xfr::log_level_t::debug>(args_to_log);

  // validate the methylome names
  const auto invalid_name =
    std::ranges::find_if_not(methylomes, &xfr::methylome::is_valid_name);
  if (invalid_name != std::cend(methylomes)) {
    lgr.error("Error: invalid methylome name \"{}\"", *invalid_name);
    return EXIT_FAILURE;
  }

  lgr.info("Initiating");

  error = [&] {
    const bool intervals_query = (bin_size == 0);
    if (intervals_query && count_covered)
      return query_intervals_cov(intervals_file, outopts, index, interface,
                                 methylomes, alt_names);
    if (intervals_query && !count_covered)
      return query_intervals(intervals_file, outopts, index, interface,
                             methylomes, alt_names);
    if (!intervals_query && count_covered)
      return query_bins_cov(bin_size, outopts, index, interface, methylomes,
                            alt_names);
    if (!intervals_query && !count_covered)
      return query_bins(bin_size, outopts, index, interface, methylomes,
                        alt_names);
    std::unreachable();
  }();

  // ADS: below is because error code '0' is printed as "Undefined
  // error" on macos.
  if (error) {
    lgr.error("Failed to complete query: {}", error);
    return EXIT_FAILURE;
  }
  lgr.info("Completed query");

  return EXIT_SUCCESS;
}
