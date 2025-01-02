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

xfrase bins local -x index_dir -g hg38 -d methylome_dir -m methylome_name  -o output.bed -b 1000
xfrase bins remote -x index_dir -g hg38 -s example.com -m SRX012345 -o output.bed -b 1000
)";

#include "client.hpp"
#include "cpg_index.hpp"
#include "cpg_index_metadata.hpp"
#include "genomic_interval_output.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_resource.hpp"
#include "request.hpp"
#include "request_type_code.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::is_same
#include <variant>      // for std::tuple
#include <vector>

namespace xfrase {

template <typename results_type>
[[nodiscard]] static inline auto
do_remote_bins(const request &req, const auto &resource,
               std::error_code &ec) -> level_container<results_type> {
  client<results_type> cl(resource.host, resource.port, req, req.bin_size());
  ec = cl.run();
  if (ec) {
    logger::instance().error("Transaction status: {}", ec);
    return {};
  }
  return cl.take_levels();
}

template <typename results_type>
[[nodiscard]] static inline auto
do_local_bins(const request &req, const auto &resource, const cpg_index &index,
              std::error_code &ec) -> level_container<results_type> {
  const auto meth = methylome::read(resource.dir, req.accession, ec);
  if (ec)
    return {};
  if constexpr (std::is_same_v<results_type, level_element_covered_t>)
    return meth.get_levels_covered(req.bin_size(), index);
  else
    return meth.get_levels(req.bin_size(), index);
}

template <typename results_type>
[[nodiscard]] auto
do_bins(const request &req, const auto &resource, const auto &outmgr,
        const cpg_index &index) -> std::error_code {
  auto &lgr = logger::instance();
  level_container<results_type> results;
  std::error_code ec;

  const auto bins_start{std::chrono::high_resolution_clock::now()};

  using given_type = std::remove_cvref_t<decltype(resource)>;
  using local_type = local_methylome_resource;
  if constexpr (std::is_same_v<given_type, local_type>)
    results = do_local_bins<results_type>(req, resource, index, ec);
  else
    results = do_remote_bins<results_type>(req, resource, ec);

  const auto bins_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for bins query: {:.3}s",
            duration(bins_start, bins_stop));
  if (ec)
    return ec;

  const auto output_start{std::chrono::high_resolution_clock::now()};
  ec = write_output(outmgr, results);
  const auto output_stop{std::chrono::high_resolution_clock::now()};
  // ADS: elapsed time for output will include conversion to scores
  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));
  return ec;
}

}  // namespace xfrase

auto
command_bins_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "bins";
  static constexpr auto default_port = "5000";
  static const auto usage =
    std::format("Usage: xfrase bins [local|remote] [options]\n");
  static const auto about_msg =
    std::format("xfrase {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  using xfrase::cpg_index;
  using xfrase::level_element_covered_t;
  using xfrase::level_element_t;
  using xfrase::log_args;
  using xfrase::log_level_t;
  using xfrase::logger;

  bool write_scores{};
  bool count_covered{};

  std::string methylome_directory{};
  std::string index_directory{};

  std::string methylome_name{};
  std::string genome_name{};

  std::uint32_t bin_size{};

  std::string hostname{};
  std::string port{};
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
    ("bin-size,b", po::value(&bin_size)->required(), "size of bins")
    ("genome,g", po::value(&genome_name)->required(), "genome name/assembly")
    ("indexdir,x", po::value(&index_directory)->required(), "local cpg index directory")
    ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
     "log level {debug,info,warning,error,critical}")
    ;
  po::options_description output("Output");
  output.add_options()
    ("output,o", po::value(&outfile)->required(), "output file")
    ("covered", po::bool_switch(&count_covered), "count covered sites per bin")
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

  auto &lgr = logger::instance(xfrase::shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  // ADS: log the command line arguments (assuming right log level)
  std::vector<std::tuple<std::string, std::string>> args_to_log{
    {"Methylome", methylome_name},
    {"Binsize", std::format("{}", bin_size)},
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

  std::error_code index_ec;
  const auto index = cpg_index::read(index_directory, genome_name, index_ec);
  if (index_ec) {
    lgr.error("Failed to read cpg index {} {}: {}", index_directory,
              genome_name, index_ec);
    return EXIT_FAILURE;
  }

  lgr.debug("Number of CpGs in index: {}", index.meta.n_cpgs);

  const auto request_type = count_covered
                              ? xfrase::request_type_code::bins_covered
                              : xfrase::request_type_code::bins;

  const auto req =
    xfrase::request{methylome_name, request_type, index.get_hash(), bin_size};

  const auto outmgr =
    xfrase::bins_output_mgr{outfile, bin_size, index, write_scores};

  // ADS: only one of these will get used
  xfrase::local_methylome_resource lmr{methylome_directory};
  xfrase::remote_methylome_resource rmr{hostname, port};

  const auto bins_err =
    count_covered
      ? (remote_mode ? do_bins<level_element_covered_t>(req, rmr, outmgr, index)
                     : do_bins<level_element_covered_t>(req, lmr, outmgr, index))
      : (remote_mode ? do_bins<level_element_t>(req, rmr, outmgr, index)
                     : do_bins<level_element_t>(req, lmr, outmgr, index));

  if (bins_err) {
    lgr.error("Error: {}", bins_err);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
