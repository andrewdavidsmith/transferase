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

xfrase intervals local -x index_dir -g hg38 -d methylome_dir -m methylome_name -o output.bed -i input.bed
xfrase intervals remote -x index_dir -g hg38 -s example.com -m methylome_name -o output.bed -i input.bed
)";

#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "genomic_interval_output.hpp"
#include "level_container.hpp"
#include "level_element.hpp"
#include "logger.hpp"
#include "methylome_resource.hpp"
#include "request.hpp"
#include "request_type_code.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <format>
#include <iostream>
#include <iterator>  // for std::size
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>
#include <vector>

auto
command_intervals_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "intervals";
  static const auto usage =
    std::format("Usage: xfrase intervals [local|remote] [options]\n");
  static const auto about_msg =
    std::format("xfrase {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  static constexpr auto default_port = "5000";

  using transferase::cpg_index;
  // using transferase::do_intervals;
  using transferase::genomic_interval;
  using transferase::level_element_covered_t;
  using transferase::level_element_t;
  using transferase::log_args;
  using transferase::log_level_t;
  using transferase::logger;

  bool write_scores{};
  bool count_covered{};

  std::string methylome_directory{};
  std::string index_directory{};

  std::string methylome_name{};
  std::string genome_name{};

  std::string intervals_file{};

  std::string port{};
  std::string hostname{};
  std::string outfile{};
  log_level_t log_level{};

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
    ("methylome,m", po::value(&methylome_name)->required(), "methylome name/accession")
    ("intervals,i", po::value(&intervals_file)->required(), "intervals file")
    ("genome,g", po::value(&genome_name)->required(), "genome name/assembly")
    ("indexdir,x", po::value(&index_directory)->required(), "local cpg index directory")
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
    ;
  po::options_description local("Local");
  local.add_options()
    ("methdir,d", po::value(&methylome_directory)->required(), "local methylome directory")
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

  auto &lgr =
    logger::instance(transferase::shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  // ADS: log the command line arguments (assuming right log level)
  std::vector<std::tuple<std::string, std::string>> args_to_log{
    {"Methylome", methylome_name},
    {"Intervals", intervals_file},
    {"Genome", genome_name},
    {"Index directory", index_directory},
    {"Output", outfile},
    {"Covered", std::format("{}", count_covered)},
    {"Bedgraph", std::format("{}", write_scores)},
  };
  std::vector<std::tuple<std::string, std::string>> remote_args{
    {"Hostname:port", std::format("{}:{}", hostname, port)},
  };
  std::vector<std::tuple<std::string, std::string>> local_args{
    {"Methylome directory", methylome_directory},
  };
  log_args<log_level_t::info>(args_to_log);
  log_args<log_level_t::info>(remote_mode ? remote_args : local_args);

  std::error_code ec;
  const auto index = cpg_index::read(index_directory, genome_name, ec);
  if (ec) {
    lgr.error("Failed to read cpg index {} {}: {}", index_directory,
              genome_name, ec);
    return EXIT_FAILURE;
  }

  // Read query intervals and validate them
  const auto intervals = genomic_interval::read(index, intervals_file, ec);
  if (ec) {
    lgr.error("Error reading intervals file: {} ({})", intervals_file, ec);
    return EXIT_FAILURE;
  }
  if (!genomic_interval::are_sorted(intervals)) {
    lgr.error("Intervals not sorted: {}", intervals_file);
    return EXIT_FAILURE;
  }
  if (!genomic_interval::are_valid(intervals)) {
    lgr.error("Intervals not valid: {} (negative size found)", intervals_file);
    return EXIT_FAILURE;
  }
  lgr.info("Number of intervals: {}", std::size(intervals));

  const auto request_type =
    count_covered ? transferase::request_type_code::intervals_covered
                  : transferase::request_type_code::intervals;

  // Convert intervals into query
  const auto format_query_start{std::chrono::high_resolution_clock::now()};
  auto query = index.make_query(intervals);
  const auto format_query_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time to prepare query: {:.3}s",
            duration(format_query_start, format_query_stop));

  const auto req = transferase::request{methylome_name, request_type,
                                        index.get_hash(), std::size(intervals)};

  const auto resource = transferase::methylome_resource{
    .directory = methylome_directory,
    .hostname = hostname,
    .port_number = port,
  };

  const auto intervals_start{std::chrono::high_resolution_clock::now()};

  const auto results =
    count_covered ? resource.get_levels<level_element_covered_t>(req, query, ec)
                  : resource.get_levels<level_element_t>(req, query, ec);
  if (ec) {
    lgr.error("Error obtaining levels: {}", ec);
    return EXIT_FAILURE;
  }

  const auto intervals_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for query: {:.3}s",
            duration(intervals_start, intervals_stop));

  const auto outmgr =
    transferase::intervals_output_mgr{outfile, intervals, index, write_scores};

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
