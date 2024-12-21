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

#include "command_check.hpp"

static constexpr auto about = R"(
check the given files for correctness and consistency
)";

static constexpr auto description = R"(
Perform 3 kinds of checks. First, the index is checked internally to
verify that the index data and the index metadata are consistent.
Second, the methylomes are each checked internally to verify that the
methylome data and methylome metadata is consistent for each given
methylome. Finally, each given methylome is checked for consistency
with the given index. Not output is written except that logged to the
console. The exit code of the app will be non-zero if any of the
consistency checks fails. At a log-level of 'debug' the outcome of
each check will be logged so the cause of any failure can be
determined.
)";

static constexpr auto examples = R"(
Examples:

xfrase check -x indexes/hg38.cpg_idx -m SRX012345.m16 SRX612345.m16
)";

#include "cpg_index.hpp"
#include "cpg_index_metadata.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_metadata.hpp"
#include "methylome_results_types.hpp"  // IWYU pragma: keep
#include "utilities.hpp"
#include "xfrase_error.hpp"  // IWYU pragma: keep

#include <boost/program_options.hpp>

#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <format>
#include <iostream>
#include <iterator>  // for std::cbegin, std::cend
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>  // IWYU pragma: keep
#include <vector>

[[nodiscard]] static auto
check_cpg_index_consistency(const cpg_index_metadata &cim,
                            const cpg_index &index) -> bool {
  auto &lgr = logger::instance();

  const auto n_cpgs_match = (cim.n_cpgs == index.get_n_cpgs());
  lgr.debug("cpg_index number of cpgs match: {}", n_cpgs_match);

  const auto hashes_match = (cim.index_hash == index.hash());
  lgr.debug("cpg_index hashes match: {}", hashes_match);

  return n_cpgs_match && hashes_match;
}

[[nodiscard]] static auto
check_methylome_consistency(const methylome_metadata &meta,
                            const methylome &meth) -> bool {
  auto &lgr = logger::instance();

  lgr.debug("methylome metadata indicates compressed: {}", meta.is_compressed);
  const auto n_cpgs_match = (meta.n_cpgs == meth.get_n_cpgs());
  lgr.debug("methylome number of cpgs match: {}", n_cpgs_match);
  const auto n_cpgs_match_ret = ((meta.is_compressed && !n_cpgs_match) ||
                                 (!meta.is_compressed && n_cpgs_match));

  const auto hashes_match = (meta.methylome_hash == meth.hash());
  lgr.debug("methylome hashes match: {}", hashes_match);
  const auto hashes_match_ret = ((meta.is_compressed && !hashes_match) ||
                                 (!meta.is_compressed && hashes_match));

  return n_cpgs_match_ret && hashes_match_ret;
}

[[nodiscard]] static auto
check_metadata_consistency(const methylome_metadata &meta,
                           const cpg_index_metadata &cim) -> bool {
  auto &lgr = logger::instance();

  const auto versions_match = (cim.version == meta.version);
  lgr.debug("metadata versions match: {}", versions_match);

  const auto index_hashes_match = (cim.index_hash == meta.index_hash);
  lgr.debug("metadata index hashes match: {}", index_hashes_match);

  const auto assemblies_match = (cim.assembly == meta.assembly);
  lgr.debug("metadata assemblies match: {}", assemblies_match);

  const auto n_cpgs_match = (cim.n_cpgs == meta.n_cpgs);
  lgr.debug("metadata assemblies match: {}", assemblies_match);

  return versions_match && index_hashes_match && assemblies_match &&
         n_cpgs_match;
}

auto
command_check_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "check";
  static const auto usage =
    std::format("Usage: xfrase {} [options]\n", strip(command));
  static const auto about_msg =
    std::format("xfrase {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  std::string index_file{};
  xfrase_log_level log_level{};

  namespace po = boost::program_options;

  po::options_description desc("Options");
  desc.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("methylomes,m", po::value<std::vector<std::string>>()->multitoken()->required(),
     "methylome files")
    ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
     "log level {debug,info,warning,error,critical}")
    // clang-format on
    ;
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help") || argc == 1) {
      std::println("{}\n{}", about_msg, usage);
      desc.print(std::cout);
      std::println("\n{}", description_msg);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    std::println("{}\n{}", about_msg, usage);
    desc.print(std::cout);
    std::println("\n{}", description_msg);
    return EXIT_FAILURE;
  }

  logger &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  const auto methylome_files = vm["methylomes"].as<std::vector<std::string>>();
  const auto index_meta_file =
    get_default_cpg_index_metadata_filename(index_file);

  const auto joined = methylome_files | std::views::join_with(',');
  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Index", index_file},
    {"Methylomes", std::string(std::cbegin(joined), std::cend(joined))},
    // clang-format on
  };
  log_args<xfrase_log_level::info>(args_to_log);

  const auto [index, cim, index_read_err] =
    read_cpg_index(index_file, index_meta_file);
  if (index_read_err) {
    lgr.error("Failed to read cpg index {} ({})", index_file, index_read_err);
    return EXIT_FAILURE;
  }
  const auto cpg_index_consitency = check_cpg_index_consistency(cim, index);
  lgr.info("Index data and metadata consistent: {}", cpg_index_consitency);

  bool all_methylomes_consitent = true;
  bool all_methylomes_metadata_consitent = true;
  for (const auto &methylome_file : methylome_files) {
    const auto methylome_meta_file =
      get_default_methylome_metadata_filename(methylome_file);
    const auto [meth, meta, meth_read_err] =
      read_methylome(methylome_file, methylome_meta_file);
    if (meth_read_err) {
      lgr.error("Failed to read methylome {} ({})", methylome_file,
                meth_read_err);
      return EXIT_FAILURE;
    }

    lgr.info("Methylome total counts: {}", meth.total_counts());
    lgr.info("Methylome total counts covered: {}", meth.total_counts_cov());

    const auto methylome_consitency = check_methylome_consistency(meta, meth);
    lgr.info("Methylome data and metadata consistent: {}",
             methylome_consitency);
    all_methylomes_consitent = all_methylomes_consitent && methylome_consitency;

    const auto metadata_consitency = check_metadata_consistency(meta, cim);
    lgr.info("Methylome and index metadata consistent: {}",
             metadata_consitency);
    all_methylomes_metadata_consitent =
      all_methylomes_metadata_consitent && metadata_consitency;
  }

  lgr.info("all methylomes consistent: {}", all_methylomes_consitent);
  lgr.info("all methylome metadata consistent: {}",
           all_methylomes_metadata_consitent);

  return cpg_index_consitency && all_methylomes_consitent &&
         all_methylomes_metadata_consitent;
}
