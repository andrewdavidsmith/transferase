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

#include "bins.hpp"

#include "client.hpp"
#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "mxe_error.hpp"
#include "request.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <tuple>
#include <utility>  // std::move
#include <vector>

using std::ofstream;
using std::string;
using std::tuple;
using std::vector;

template <typename counts_res_type>
[[nodiscard]] static inline auto
do_remote_bins(const string &accession, const cpg_index &index,
               const std::uint32_t bin_size, const string &hostname,
               const string &port)
  -> tuple<vector<counts_res_type>, std::error_code> {
  request_header hdr{accession, index.n_cpgs_total, {}};

  if constexpr (std::is_same<counts_res_type, counts_res>::value)
    hdr.rq_type = request_header::request_type::bin_counts;
  else
    hdr.rq_type = request_header::request_type::bin_counts_cov;

  bins_request req{bin_size};
  mxe_client<counts_res_type, bins_request> mxec(hostname, port, hdr, req,
                                                 logger::instance());
  const auto status = mxec.run();
  if (status) {
    mxe_log_error("Transaction status: {}", status);
    return {{}, status};
  }

  return {std::move(mxec.take_counts()), {}};
}

template <typename counts_res_type>
[[nodiscard]] static inline auto
do_local_bins(const string &meth_file, const string &meta_file,
              const cpg_index &index, const std::uint32_t bin_size)
  -> tuple<vector<counts_res_type>, std::error_code> {
  const auto [meta, meta_err] = methylome_metadata::read(meta_file);
  if (meta_err) {
    mxe_log_error("Error: {} ({})", meta_err, meta_file);
    return {{}, meta_err};
  }

  methylome meth{};
  const auto meth_read_err = meth.read(meth_file, meta);
  if (meth_read_err) {
    mxe_log_error("Error: {} ({})", meth_read_err, meth_file);
    return {{}, meth_read_err};
  }
  if constexpr (std::is_same<counts_res_type, counts_res_cov>::value)
    return {std::move(meth.get_bins_cov(bin_size, index)), {}};
  else
    return {std::move(meth.get_bins(bin_size, index)), {}};
}

template <typename counts_res_type>
static auto
do_bins(const string &accession, const cpg_index &index,
        const std::uint32_t bin_size, const string &hostname,
        const string &port, const string &meth_file, const string &meta_file,
        std::ostream &out, const bool write_scores,
        const bool remote_mode) -> std::error_code {
  using hr_clock = std::chrono::high_resolution_clock;

  const auto bins_start{hr_clock::now()};
  const auto [results, bins_err] =
    remote_mode
      ? do_remote_bins<counts_res_type>(accession, index, bin_size, hostname,
                                        port)
      : do_local_bins<counts_res_type>(meth_file, meta_file, index, bin_size);

  const auto bins_stop{hr_clock::now()};
  mxe_log_debug("Elapsed time for bins query: {:.3}s",
                duration(bins_start, bins_stop));

  if (bins_err)  // ADS: error messages already logged
    return std::make_error_code(std::errc::invalid_argument);

  const auto output_start{hr_clock::now()};
  write_bins(out, bin_size, index, results);
  const auto output_stop{hr_clock::now()};
  // ADS: elapsed time for output will include conversion to scores
  mxe_log_debug("Elapsed time for output: {:.3}s",
                duration(output_start, output_stop));
  return {};
}

auto
bins_main(int argc, char *argv[]) -> int {
  static constexpr mxe_log_level default_log_level = mxe_log_level::info;
  static constexpr auto usage_format =
    "Usage: mxe lookup {} [options]\n\nOption groups";
  static constexpr auto default_port = "5000";
  static constexpr auto command = "bins";

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

  po::variables_map vm_subcmd;
  po::store(po::command_line_parser(argc, argv)
              .options(subcmds)
              .positional(p)
              .allow_unregistered()
              .run(),
            vm_subcmd);
  po::notify(vm_subcmd);

  if (subcmd != "local" && subcmd != "remote") {
    std::println("Usage: mxe lookup [local|remote] [options]");
    return EXIT_FAILURE;
  }
  const bool remote_mode = subcmd == "remote";

  po::options_description general("General");
  // clang-format off
  general.add_options()
    ("help,h", "produce help message")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("bin-size,b", po::value(&bin_size)->required(), "size of bins")
    ("log-level,v", po::value(&log_level)->default_value(default_log_level),
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

  po::options_description all(std::format(usage_format, subcmd));
  all.add(general).add(output);
  all.add(remote_mode ? remote : local);

  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc - 1, argv + 1, all), vm);
    if (vm.count("help") || argc <= 2) {
      all.print(std::cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    all.print(std::cout);
    return EXIT_FAILURE;
  }

  if (meta_file.empty())
    meta_file = get_default_methylome_metadata_filename(meth_file);

  logger &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  // ADS: log the command line arguments (assuming right log level)
  vector<tuple<string, string>> args_to_log{
    {"Index", index_file},
    {"Binsize", std::format("{}", bin_size)},
    {"Output", outfile},
    {"Covered", std::format("{}", count_covered)},
    {"Bedgraph", std::format("{}", write_scores)},
  };
  vector<tuple<string, string>> remote_args{
    {"Hostname:port", std::format("{}:{}", hostname, port)},
    {"Accession", accession},
  };
  vector<tuple<string, string>> local_args{
    {"Methylome", meth_file},
    {"Metadata", meta_file},
  };
  log_args<mxe_log_level::info>(args_to_log);
  log_args<mxe_log_level::info>(remote_mode ? remote_args : local_args);

  cpg_index index{};
  if (const auto index_read_err = index.read(index_file); index_read_err) {
    lgr.error("Failed to read cpg index: {} ({})", index_file, index_read_err);
    return EXIT_FAILURE;
  }

  if (log_level == mxe_log_level::debug)
    lgr.debug("Number of CpGs in index: {}", index.n_cpgs_total);

  ofstream out(outfile);
  if (!out) {
    lgr.error("Failed to open output file: {}", outfile);
    return EXIT_FAILURE;
  }

  const auto bins_err =
    count_covered
      ? do_bins<counts_res_cov>(accession, index, bin_size, hostname, port,
                                meth_file, meta_file, out, write_scores,
                                remote_mode)
      : do_bins<counts_res>(accession, index, bin_size, hostname, port,
                            meth_file, meta_file, out, write_scores,
                            remote_mode);

  return bins_err == std::errc() ? EXIT_SUCCESS : EXIT_FAILURE;
}
