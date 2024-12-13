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
The 'bins' command accepts a bin size and a methylome, and it
generates a summary of the methylation levels in each non-overlapping
bin of the given size. This command runs in two modes, local and
remote. The local mode is for analyzing data on your local storage:
either your own data or data that you downloaded. The remote mode is
for analyzing methylomes in a remote database on a server. Depending
on the mode you select, the options you must specify will differ.
)";

static constexpr auto examples = R"(
Examples:

mxe bins local -x hg38.cpg_idx -o output.bed -m methylome.m16 -b 1000
mxe bins remote -x hg38.cpg_idx -o output.bed -s example.com -a SRX012345 -b 1000
)";

#include "client.hpp"
#include "cpg_index.hpp"
#include "cpg_index_meta.hpp"
#include "genomic_interval_output.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_metadata.hpp"
#include "methylome_results_types.hpp"
#include "request.hpp"
#include "utilities.hpp"
// #include "mxe_error.hpp"

#include <boost/program_options.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>  // for std::views
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::is_same
#include <utility>      // for std::move
#include <variant>      // for std::tuple
#include <vector>

using std::string;
using std::vector;

template <typename counts_res_type>
[[nodiscard]] static inline auto
do_remote_bins(const string &accession, const cpg_index_meta &cim,
               const std::uint32_t bin_size, const string &hostname,
               const string &port)
  -> std::tuple<vector<counts_res_type>, std::error_code> {
  request_header hdr{accession, cim.n_cpgs, {}};

  if constexpr (std::is_same<counts_res_type, counts_res>::value)
    hdr.rq_type = request_header::request_type::bin_counts;
  else
    hdr.rq_type = request_header::request_type::bin_counts_cov;

  bins_request req{bin_size};
  mxe_client<counts_res_type, bins_request> mxec(hostname, port, hdr, req);
  const auto status = mxec.run();
  if (status) {
    logger::instance().error("Transaction status: {}", status);
    return {{}, status};
  }
  return {std::move(mxec.take_counts()), {}};
}

template <typename counts_res_type>
[[nodiscard]] static inline auto
do_local_bins(const string &meth_file, const string &meta_file,
              const cpg_index &index, const cpg_index_meta &cim,
              const std::uint32_t bin_size)
  -> std::tuple<vector<counts_res_type>, std::error_code> {
  const auto [meta, meta_err] = methylome_metadata::read(meta_file);
  if (meta_err) {
    logger::instance().error("Error: {} ({})", meta_err, meta_file);
    return {{}, meta_err};
  }

  const auto [meth, meth_read_err] = methylome::read(meth_file, meta);
  if (meth_read_err) {
    logger::instance().error("Error: {} ({})", meth_read_err, meth_file);
    return {{}, meth_read_err};
  }
  if constexpr (std::is_same<counts_res_type, counts_res_cov>::value)
    return {std::move(meth.get_bins_cov(bin_size, index, cim)), {}};
  else
    return {std::move(meth.get_bins(bin_size, index, cim)), {}};
}

[[nodiscard]] static inline auto
write_output(std::ostream &out, const cpg_index_meta &cim,
             const std::uint32_t bin_size, const auto &results,
             const bool write_scores) {
  if (write_scores) {
    // ADS: counting intervals that have no reads
    std::uint32_t zero_coverage = 0;
    const auto to_score = [&zero_coverage](const auto &x) {
      zero_coverage += (x.n_meth + x.n_unmeth == 0);
      return x.n_meth /
             std::max(1.0, static_cast<double>(x.n_meth + x.n_unmeth));
    };
    logger::instance().debug("Number of bins without reads: {}", zero_coverage);
    const auto scores = std::views::transform(results, to_score);
    return write_bins_bedgraph(out, cim, bin_size, scores);
  }
  else
    return write_bins(out, cim, bin_size, results);
}

template <typename counts_res_type>
static auto
do_bins(const string &accession, const cpg_index &index,
        const cpg_index_meta &cim, const std::uint32_t bin_size,
        const string &hostname, const string &port, const string &meth_file,
        const string &meta_file, std::ostream &out, const bool write_scores,
        const bool remote_mode) -> std::error_code {
  logger &lgr = logger::instance();
  const auto bins_start{std::chrono::high_resolution_clock::now()};
  const auto [results, bins_err] =
    remote_mode ? do_remote_bins<counts_res_type>(accession, cim, bin_size,
                                                  hostname, port)
                : do_local_bins<counts_res_type>(meth_file, meta_file, index,
                                                 cim, bin_size);
  const auto bins_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for bins query: {:.3}s",
            duration(bins_start, bins_stop));

  if (bins_err)  // ADS: error messages already logged
    return std::make_error_code(std::errc::invalid_argument);

  const auto output_start{std::chrono::high_resolution_clock::now()};
  const auto write_err =
    write_output(out, cim, bin_size, results, write_scores);
  if (write_err)
    return write_err;
  const auto output_stop{std::chrono::high_resolution_clock::now()};
  // ADS: elapsed time for output will include conversion to scores
  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));
  return {};
}

auto
command_bins_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "bins";
  static const auto usage =
    std::format("Usage: mxe {} [options]\n", strip(command));
  static const auto about_msg =
    std::format("mxe {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  static constexpr auto default_port = "5000";

  string accession{};
  string hostname{};
  string index_file{};
  string meta_file{};
  string meth_file{};
  string outfile{};
  string port{};
  string subcmd;
  bool count_covered{};
  bool write_scores{};
  mxe_log_level log_level{};
  std::uint32_t bin_size{};

  namespace po = boost::program_options;

  po::options_description subcmds;
  subcmds.add_options()
    // clang-format off
    ("subcmd", po::value(&subcmd))
    ("subargs", po::value<vector<string>>())
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
    ("bin-size,b", po::value(&bin_size)->required(), "size of bins")
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
    ("accession,a", po::value(&accession)->required(), "methylome accession")
    ;
  po::options_description local("Local");
  local.add_options()
    ("methylome,m", po::value(&meth_file)->required(), "local methylome file")
    ("meta", po::value(&meta_file), "methylome metadata file")
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

  po::options_description all("Options");
  if (subcmd == "local")
    all.add(general).add(output).add(local);
  else if (subcmd == "remote")
    all.add(general).add(output).add(remote);
  else
    all.add(general).add(output).add(local).add(remote);

  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc - 1, argv + 1, all), vm);
    if (vm.count("help") || argc == 1 || (argc == 2 && !subcmd.empty())) {
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

  if (meta_file.empty())
    meta_file = get_default_methylome_metadata_filename(meth_file);

  logger &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  // ADS: log the command line arguments (assuming right log level)
  vector<std::tuple<string, string>> args_to_log{
    {"Index", index_file},
    {"Binsize", std::format("{}", bin_size)},
    {"Output", outfile},
    {"Covered", std::format("{}", count_covered)},
    {"Bedgraph", std::format("{}", write_scores)},
  };
  vector<std::tuple<string, string>> remote_args{
    {"Hostname:port", std::format("{}:{}", hostname, port)},
    {"Accession", accession},
  };
  vector<std::tuple<string, string>> local_args{
    {"Methylome", meth_file},
    {"Metadata", meta_file},
  };
  log_args<mxe_log_level::info>(args_to_log);
  log_args<mxe_log_level::info>(remote_mode ? remote_args : local_args);

  const auto [index, cim, index_read_err] = read_cpg_index(index_file);
  if (index_read_err) {
    lgr.error("Failed to read cpg index: {} ({})", index_file, index_read_err);
    return EXIT_FAILURE;
  }

  lgr.debug("Number of CpGs in index: {}", cim.n_cpgs);

  std::ofstream out(outfile);
  if (!out) {
    lgr.error("Failed to open output file {}: {}", outfile,
              std::make_error_code(std::errc(errno)));
    return EXIT_FAILURE;
  }

  const auto bins_err =
    count_covered
      ? do_bins<counts_res_cov>(accession, index, cim, bin_size, hostname, port,
                                meth_file, meta_file, out, write_scores,
                                remote_mode)
      : do_bins<counts_res>(accession, index, cim, bin_size, hostname, port,
                            meth_file, meta_file, out, write_scores,
                            remote_mode);

  return bins_err == std::errc() ? EXIT_SUCCESS : EXIT_FAILURE;
}
