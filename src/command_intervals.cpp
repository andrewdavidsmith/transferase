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

xfrase intervals local -x hg38.cpg_idx -o output.bed -m methylome.m16 -i input.bed
xfrase intervals remote -x hg38.cpg_idx -o output.bed -s example.com -a SRX012345 -i input.bed
)";

#include "client.hpp"
#include "cpg_index.hpp"
#include "cpg_index_meta.hpp"
#include "genomic_interval.hpp"
#include "genomic_interval_output.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_metadata.hpp"
#include "methylome_results_types.hpp"
// #include "xfrase_error.hpp"
#include "request.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>  // for std::uint32_t
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>  // for std::size
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::is_same
#include <utility>
#include <variant>
#include <vector>

template <typename counts_res_type>
[[nodiscard]] static inline auto
do_remote_intervals(const std::string &accession, const cpg_index_meta &cim,
                    std::vector<methylome::offset_pair> offsets,
                    const std::string &hostname, const std::string &port)
  -> std::tuple<std::vector<counts_res_type>, std::error_code> {
  request_header hdr{accession, cim.n_cpgs, {}};

  if constexpr (std::is_same<counts_res_type, counts_res>::value)
    hdr.rq_type = request_header::request_type::counts;
  else
    hdr.rq_type = request_header::request_type::counts_cov;

  request req{static_cast<std::uint32_t>(size(offsets)), offsets};
  xfrase::client<counts_res_type, request> cl(hostname, port, hdr, req);
  const auto status = cl.run();
  if (status) {
    logger::instance().error("Transaction status: {}", status);
    return {{}, status};
  }
  return {std::move(cl.take_counts()), {}};
}

template <typename counts_res_type>
[[nodiscard]] static inline auto
do_local_intervals(const std::string &meth_file,
                   const std::string &meth_meta_file,
                   std::vector<methylome::offset_pair> offsets)
  -> std::tuple<std::vector<counts_res_type>, std::error_code> {
  logger &lgr = logger::instance();
  const auto [meta, meta_err] = methylome_metadata::read(meth_meta_file);
  if (meta_err) {
    lgr.error("Error reading file {}: {}", meth_meta_file, meta_err);
    return {{}, meta_err};
  }
  const auto [meth, meth_err] = methylome::read(meth_file, meta);
  if (meth_err) {
    lgr.error("Error reading file {}: {}", meth_file, meth_err);
    return {{}, meth_err};
  }
  if constexpr (std::is_same<counts_res_type, counts_res_cov>::value)
    return {std::move(meth.get_counts_cov(offsets)), {}};
  else
    return {std::move(meth.get_counts(offsets)), {}};
}

template <typename counts_res_type>
static auto
do_intervals(const std::string &accession, const cpg_index_meta &cim,
             const std::vector<methylome::offset_pair> &offsets,
             const std::string &hostname, const std::string &port,
             const std::string &meth_file, const std::string &meth_meta_file,
             const std::string &outfile,
             const std::vector<genomic_interval> &gis, const bool write_scores,
             const bool remote_mode) -> std::error_code {
  const auto intervals_start{std::chrono::high_resolution_clock::now()};
  const auto [results, intervals_err] =
    remote_mode
      ? do_remote_intervals<counts_res_type>(accession, cim, offsets, hostname,
                                             port)
      : do_local_intervals<counts_res_type>(meth_file, meth_meta_file, offsets);
  const auto intervals_stop{std::chrono::high_resolution_clock::now()};
  logger::instance().debug("Elapsed time for query: {:.3}s",
                           duration(intervals_start, intervals_stop));

  if (intervals_err)  // ADS: error messages already logged
    return std::make_error_code(std::errc::invalid_argument);

  const auto output_start{std::chrono::high_resolution_clock::now()};
  const auto write_err = write_output(outfile, gis, cim, results, write_scores);
  const auto output_stop{std::chrono::high_resolution_clock::now()};
  // ADS: elapsed time for output will include conversion to scores
  logger::instance().debug("Elapsed time for output: {:.3}s",
                           duration(output_start, output_stop));
  return write_err;
}

auto
command_intervals_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "intervals";
  static const auto usage =
    std::format("Usage: xfrase intervals [local|remote] [options]\n");
  static const auto about_msg =
    std::format("xfrase {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  static constexpr auto default_port = "5000";

  bool write_scores{};
  bool count_covered{};
  std::string port{};
  std::string accession{};
  std::string index_file{};
  std::string meth_file{};
  std::string meth_meta_file{};
  std::string intervals_file{};
  std::string hostname{};
  std::string outfile{};
  xfrase_log_level log_level{};

  std::string subcmd;

  namespace po = boost::program_options;

  po::options_description subcmds;
  subcmds.add_options()
    // clang-format off
    ("subcmd", po::value(&subcmd))
    ("subargs", po::value<std::vector<std::string>>())
    // clang-format on
    ;
  // positional; one for "subcmd" and the rest else parser throws
  po::positional_options_description p;
  p.add("subcmd", 1).add("subargs", -1);

  po::options_description general("General");
  // clang-format off
  general.add_options()
    ("help,h", "print this message and exit")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("intervals,i", po::value(&intervals_file)->required(), "intervals file")
    ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
     "log level {debug,info,warning,error,critical}")
    ;
  po::options_description output("Output");
  output.add_options()
    ("output,o", po::value(&outfile)->required(), "output file")
    ("covered", po::bool_switch(&count_covered), "count covered sites per interval")
    ("score", po::bool_switch(&write_scores), "weighted methylation bedgraph format")
    ;
  po::options_description remote("Remote");
  remote.add_options()
    ("hostname,s", po::value(&hostname)->required(), "server hostname")
    ("port,p", po::value(&port)->default_value(default_port), "port")
    ("accession,a", po::value(&accession)->required(), "methylome accession")
    ;
  po::options_description local("Local");
  local.add_options()
    ("methylome,m", po::value(&meth_file)->required(), "local methylome file")
    ("metadata", po::value(&meth_meta_file), "methylome metadata (default: methylome.json)")
    ;
  // clang-format on

  po::variables_map vm_subcmd;
  po::store(po::command_line_parser(argc, argv)
              .options(subcmds)
              .positional(p)
              .allow_unregistered()
              .run(),
            vm_subcmd);
  po::notify(vm_subcmd);

  bool force_help_message{};
  po::options_description all("Options");
  if (subcmd == "local")
    all.add(general).add(output).add(local);
  else if (subcmd == "remote")
    all.add(general).add(output).add(remote);
  else {
    force_help_message = true;
    all.add(general).add(output).add(local).add(remote);
  }

  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc - 1, argv + 1, all), vm);
    if (force_help_message || vm.count("help") || argc == 1 ||
        (argc == 2 && !subcmd.empty())) {
      if (!subcmd.empty() && subcmd != "local" && subcmd != "remote")
        std::println("One of local or remote must be specified\n");
      std::println("{}\n{}", about_msg, usage);
      all.print(std::cout);
      std::println("\n{}", description_msg);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    std::println("{}\n{}", about_msg, usage);
    all.print(std::cout);
    std::println("\n{}", description_msg);
    return EXIT_FAILURE;
  }

  const bool remote_mode = (subcmd == "remote");

  if (meth_meta_file.empty())
    meth_meta_file = get_default_methylome_metadata_filename(meth_file);

  logger &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  // ADS: log the command line arguments (assuming right log level)
  std::vector<std::tuple<std::string, std::string>> args_to_log{
    {"Index", index_file},
    {"Intervals", intervals_file},
    {"Output", outfile},
    {"Covered", std::format("{}", count_covered)},
    {"Bedgraph", std::format("{}", write_scores)},
  };
  std::vector<std::tuple<std::string, std::string>> remote_args{
    {"Hostname:port", std::format("{}:{}", hostname, port)},
    {"Accession", accession},
  };
  std::vector<std::tuple<std::string, std::string>> local_args{
    {"Methylome", meth_file},
    {"Metadata", meth_meta_file},
  };
  log_args<xfrase_log_level::info>(args_to_log);
  log_args<xfrase_log_level::info>(remote_mode ? remote_args : local_args);

  const auto [index, cim, index_read_err] = read_cpg_index(index_file);
  if (index_read_err) {
    lgr.error("Failed to read cpg index: {} ({})", index_file, index_read_err);
    return EXIT_FAILURE;
  }

  lgr.debug("Number of CpGs in index: {}", cim.n_cpgs);

  // Read query intervals and validate them
  const auto [gis, ec] = genomic_interval::load(cim, intervals_file);
  if (ec) {
    lgr.error("Error reading intervals file: {} ({})", intervals_file, ec);
    return EXIT_FAILURE;
  }
  if (!intervals_sorted(cim, gis)) {
    lgr.error("Intervals not sorted: {}", intervals_file);
    return EXIT_FAILURE;
  }
  if (!intervals_valid(gis)) {
    lgr.error("Intervals not valid: {} (negative size found)", intervals_file);
    return EXIT_FAILURE;
  }
  lgr.info("Number of intervals: {}", size(gis));

  // Convert intervals into offsets
  const auto get_offsets_start{std::chrono::high_resolution_clock::now()};
  const auto offsets = index.get_offsets(cim, gis);
  const auto get_offsets_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time to get offsets: {:.3}s",
            duration(get_offsets_start, get_offsets_stop));

  const auto intervals_err =
    count_covered
      ? do_intervals<counts_res_cov>(accession, cim, offsets, hostname, port,
                                     meth_file, meth_meta_file, outfile, gis,
                                     write_scores, remote_mode)
      : do_intervals<counts_res>(accession, cim, offsets, hostname, port,
                                 meth_file, meth_meta_file, outfile, gis,
                                 write_scores, remote_mode);
  if (intervals_err) {
    lgr.error("Error: {}", intervals_err);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
