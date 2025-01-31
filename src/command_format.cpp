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

#include "command_format.hpp"

static constexpr auto about = R"(
convert single-CpG methylation levels into methylome format
)";

static constexpr auto description = R"(
The methylome format is a small representation of single-CpG
methylation levels that allows for summary statistics to be quickly
computed for genomic intervals. The methylome format involves two
files.  The methylome data is a binary file with size just over 100MB
for the human genome and it should have the extension '.m16'. The
methylome metadata is a small JSON format file (on a single line) that
can easily be examined with any JSON formatter (e.g., jq or
json_pp). These two files reside in the same directory. If xfr is
used remotely, the methylome will reside on the server. If you are
analyzing your own DNA methylation data, you will need to format your
methylomes with this command.
)";

static constexpr auto examples = R"(
Examples:

xfr format -g hg38 -d output_dir -m SRX012345.xsym.gz
)";

#include "arguments.hpp"
#include "client_config.hpp"
#include "counts_file_format.hpp"
#include "format_error_code.hpp"  // IWYU pragma: keep
#include "genome_index.hpp"
#include "genome_index_data.hpp"
#include "genome_index_metadata.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_data.hpp"
#include "methylome_metadata.hpp"
#include "utilities.hpp"
#include "zlib_adapter.hpp"

#include <boost/program_options.hpp>

#include <cctype>  // for std::isdigit
#include <charconv>
#include <chrono>
#include <cstdint>  // for std::uint32_t, std::uint64_t, std::int32_t
#include <cstdlib>  // for EXIT_FAILURE, abort, EXIT_SUCCESS
#include <format>
#include <iterator>  // for std::cbegin, std::size
#include <limits>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::true_type
#include <unordered_map>
#include <utility>
#include <variant>  // IWYU pragma: keep
#include <vector>

enum class counts_file_format_error_code : std::uint8_t {
  // clang-format off
  ok                                         = 0,
  xcounts_file_open_failure                  = 1,
  xcounts_file_header_failure                = 2,
  xcounts_file_chromosome_not_found          = 3,
  xcounts_file_incorrect_chromosome_size     = 4,
  // clang-format on
};

template <>
struct std::is_error_code_enum<counts_file_format_error_code>
  : public std::true_type {};

struct counts_file_format_error_code_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "counts_file_format_error_code"; }
  auto message(const int condition) const -> std::string override {
    using std::string_literals::operator""s;
    switch (condition) {
    case 0: return "ok"s;
    case 1: return "failed to open methylome file"s;
    case 2: return "failed to parse xcounts header"s;
    case 3: return "failed to find chromosome in xcounts header"s;
    case 4: return "incorrect chromosome size"s;
    }
    std::unreachable();  // unreachable
  }
  // clang-format on
};

inline auto
make_error_code(counts_file_format_error_code e) -> std::error_code {
  static auto category = counts_file_format_error_code_category{};
  return std::error_code(std::to_underlying(e), category);
}

namespace transferase {

static inline auto
skip_absent_cpgs(const std::uint64_t end_pos, const genome_index_data::vec &idx,
                 std::uint32_t cpg_idx_in) -> std::uint32_t {
  const std::uint32_t first_idx = cpg_idx_in;
  while (cpg_idx_in < std::size(idx) && idx[cpg_idx_in] < end_pos)
    ++cpg_idx_in;
  return cpg_idx_in - first_idx;
}

static inline auto
get_ch_id(const genome_index_metadata &meta,
          const std::string &chrom_name) -> std::int32_t {
  const auto ch_id = meta.chrom_index.find(chrom_name);
  if (ch_id == cend(meta.chrom_index))
    return -1;
  return ch_id->second;
}

// ADS: this function is tied to the specifics of dnmtools::xcounts
// code, and likely needs a review
static auto
verify_header_line(const genome_index_metadata &meta,
                   const std::string &line) -> std::error_code {
  using namespace std::literals;  // NOLINT
  static constexpr auto DNMTOOLS_IDENTIFIER = "#DNMTOOLS"sv;
  // ignore the version line and the header end line
  if (line.starts_with(DNMTOOLS_IDENTIFIER) || std::size(line) == 1)
    return counts_file_format_error_code::ok;

  // parse the chrom and its size
  std::string chrom;
  std::uint64_t chrom_size{};
  std::istringstream iss{line};
  if (!(iss >> chrom >> chrom_size))
    return counts_file_format_error_code::xcounts_file_header_failure;

  chrom = chrom.substr(1);  // remove leading '#'

  // validate the chromosome order is consistent between the index and
  // methylome transferase file
  const auto order_itr = meta.chrom_index.find(chrom);
  if (order_itr == cend(meta.chrom_index))
    return counts_file_format_error_code::xcounts_file_chromosome_not_found;

  // validate that the chromosome size is the same between the index
  // and the methylome transferase file
  const auto size_itr = meta.chrom_size[order_itr->second];
  if (chrom_size != size_itr)
    return counts_file_format_error_code::
      xcounts_file_incorrect_chromosome_size;

  return counts_file_format_error_code::ok;
}

static auto
process_cpg_sites_xcounts(const std::string &infile, const genome_index &index)
  -> std::tuple<methylome_data, std::error_code> {
  auto &lgr = transferase::logger::instance();

  const genome_index_metadata &index_meta = index.meta;
  const auto begin_positions = std::cbegin(index.data.positions);

  std::error_code err;
  gzinfile mf(infile, err);
  if (err != zlib_adapter_error_code::ok) {
    lgr.error("Error reading xcounts file: {}", infile);
    return {methylome_data{}, err};
  }

  std::uint32_t cpg_idx_in{};  // index of current input cpg position
  std::uint64_t pos = std::numeric_limits<std::uint64_t>::max();

  std::vector<genome_index_data::vec>::const_iterator positions{};

  // ADS: if optimization is needed, this can be flattened here to
  // avoid a copy later
  std::vector<methylome_data::vec> cpgs;
  for (const auto n_cpgs_chrom : index_meta.get_n_cpgs_chrom())
    // cppcheck-suppress useStlAlgorithm
    cpgs.push_back(methylome_data::vec(n_cpgs_chrom));

  std::uint32_t cpg_idx_out{};

  methylome_data::vec::iterator cpgs_itr;

  std::string line;
  while (mf.getline(line)) {
    if (line[0] == '#') {
      // consistency check between reference used for the index and
      // reference used for the methylome
      if (err = verify_header_line(index_meta, line); err) {
        lgr.error("Error parsing xcounts header line: {} ({})", line, err);
        return {methylome_data{}, err};
      }
      continue;  // ADS: early loop exit
    }
    if (!std::isdigit(line[0])) {  // check for new chromosome
      const std::int32_t ch_id = get_ch_id(index_meta, line);
      if (ch_id < 0) {
        lgr.error("Failed to find chromosome in index: {}", line);
        return {
          methylome_data{},
          counts_file_format_error_code::xcounts_file_chromosome_not_found};
      }
      cpg_idx_out = 0;

      positions = begin_positions + ch_id;
      pos = 0;         // position in genome
      cpg_idx_in = 0;  // index of current cpg site

      cpgs_itr = std::begin(cpgs[ch_id]);
    }
    else {
      std::uint32_t pos_step = 0, n_meth = 0, n_unmeth = 0;
      // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      const auto end_line = line.data() + std::size(line);
      auto res = std::from_chars(line.data(), end_line, pos_step);
      res = std::from_chars(res.ptr + 1, end_line, n_meth);
      res = std::from_chars(res.ptr + 1, end_line, n_unmeth);
      // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      // ADS: check for errors here

      const auto curr_pos = pos + pos_step;
      if (pos + 1 < curr_pos) {
        const auto n_skips = skip_absent_cpgs(curr_pos, *positions, cpg_idx_in);
        cpg_idx_out += n_skips;
        cpg_idx_in += n_skips;
      }

      // ADS: prevent counts from overflowing
      conditional_round_to_fit<mcount_t>(n_meth, n_unmeth);

      *(cpgs_itr + cpg_idx_out++) = {n_meth, n_unmeth};

      pos = curr_pos;
      ++cpg_idx_in;
    }
  }

  methylome_data::vec cpgs_flat;
  cpgs_flat.reserve(index_meta.n_cpgs);
  for (const auto &c : cpgs)
    cpgs_flat.insert(std::end(cpgs_flat), std::cbegin(c), std::cend(c));

  // tuple is move-returned, but making the tuple would copy
  return {methylome_data{std::move(cpgs_flat)}, std::error_code{}};
}

static auto
process_cpg_sites_counts(const std::string &infile, const genome_index &index)
  -> std::tuple<methylome_data, std::error_code> {
  auto &lgr = transferase::logger::instance();

  const genome_index_metadata &index_meta = index.meta;
  const auto begin_positions = std::cbegin(index.data.positions);

  std::error_code err;
  gzinfile mf(infile, err);
  if (err != zlib_adapter_error_code::ok) {
    lgr.error("Error reading counts file: {}", infile);
    return {methylome_data{}, err};
  }

  std::uint32_t cpg_idx_in{};  // index of current input cpg position
  std::uint64_t pos = std::numeric_limits<std::uint64_t>::max();

  std::vector<genome_index_data::vec>::const_iterator positions{};

  // ADS: if optimization is needed, this can be flattened here to
  // avoid a copy later
  std::vector<methylome_data::vec> cpgs;
  for (const auto n_cpgs_chrom : index_meta.get_n_cpgs_chrom())
    // cppcheck-suppress useStlAlgorithm
    cpgs.push_back(methylome_data::vec(n_cpgs_chrom));

  std::uint32_t cpg_idx_out{};

  methylome_data::vec::iterator cpgs_itr;

  std::string prev_chrom;
  std::string line;
  bool parse_success{true};
  while (parse_success && mf.getline(line)) {
    if (line[0] == '#')
      continue;
    const auto end_of_chrom = line.find_first_of(" \t");
    std::string chrom{line.substr(0, end_of_chrom)};
    if (chrom != prev_chrom) {  // check for new chromosome
      const std::int32_t ch_id = get_ch_id(index_meta, chrom);
      if (ch_id < 0) {
        lgr.error("Failed to find chromosome in index: {}", line);
        return {
          methylome_data{}, /* ADS: fix this */
          counts_file_format_error_code::xcounts_file_chromosome_not_found};
      }
      cpg_idx_out = 0;

      positions = begin_positions + ch_id;
      pos = 0;         // position in genome
      cpg_idx_in = 0;  // index of current cpg site

      cpgs_itr = std::begin(cpgs[ch_id]);
      prev_chrom = std::move(chrom);
    }
    std::uint32_t curr_pos = 0, n_meth = 0, n_unmeth = 0;
    parse_success = parse_counts_line(line, curr_pos, n_meth, n_unmeth);

    if (pos + 1 < curr_pos) {
      const auto n_skips = skip_absent_cpgs(curr_pos, *positions, cpg_idx_in);
      cpg_idx_out += n_skips;
      cpg_idx_in += n_skips;
    }

    // ADS: prevent counts from overflowing
    conditional_round_to_fit<mcount_t>(n_meth, n_unmeth);

    *(cpgs_itr + cpg_idx_out++) = {n_meth, n_unmeth};

    pos = curr_pos;
    ++cpg_idx_in;
  }

  methylome_data::vec cpgs_flat;
  cpgs_flat.reserve(index_meta.n_cpgs);
  for (const auto &c : cpgs)
    cpgs_flat.insert(std::end(cpgs_flat), std::cbegin(c), std::cend(c));

  // tuple is move-returned, but making the tuple would copy
  return {methylome_data{std::move(cpgs_flat)}, std::error_code{}};
}

struct command_format_argset : argset_base<command_format_argset> {
  [[nodiscard]] static auto
  get_default_config_file_impl() -> std::string {
    std::error_code ec;
    return client_config::get_config_file_default(ec);
  }

  std::string hostname{};
  std::string port{};
  std::string log_filename{};
  std::string labels_dir{};

  std::string index_directory{};
  std::string genome_name{};

  std::string methylation_input{};
  std::string methylome_name{};
  std::string methylome_outdir{};
  log_level_t log_level{};
  bool zip{false};

  auto
  log_options_impl() const {
    transferase::log_args<log_level_t::info>(
      std::vector<std::tuple<std::string, std::string>>{
        // clang-format off
        {"Methylation", methylation_input},
        {"Methylome name", methylome_name},
        {"Genome", genome_name},
        {"Index directory", index_directory},
        {"Methylome directory", methylome_outdir},
        {"Zip", std::format("{}", zip)},
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
      ("hostname", po::value(&hostname), "none")
      ("port", po::value(&port), "none")
      ("log-file", po::value(&log_filename), "none")
      // clang-format on
      ;
    return opts;
  }

  [[nodiscard]] auto
  set_opts_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    using po::value;
    boost::program_options::options_description opts("Options");
    opts.add_options()
      // clang-format off
      ("help,h", "print this message and exit")
      ("config-file,c",
       po::value(&config_file)->default_value(get_default_config_file(), ""),
       "use specified config file")
      ("meth-file,m", po::value(&methylation_input)->required(),
       "methylation input file")
      ("index-dir,x", po::value(&index_directory),
       "genome index directory")
      ("methylome-dir,d", po::value(&methylome_outdir)->required(),
       "methylome output directory")
      ("genome,g", po::value(&genome_name)->required(), "genome name")
      ("zip,z", po::bool_switch(&zip), "zip the output")
      ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
       "{debug, info, warning, error, critical}")
      // clang-format on
      ;
    return opts;
  }
};

}  // namespace transferase

auto
command_format_main(int argc,
                    char *argv[])  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  -> int {
  const auto command_start = std::chrono::high_resolution_clock::now();

  static constexpr auto command = "format";
  static const auto usage =
    std::format("Usage: xfr {} [options]\n", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  transferase::command_format_argset args;
  auto ec = args.parse(argc, argv, usage, about_msg, description_msg);
  if (ec == argument_error_code::help_requested)
    return EXIT_SUCCESS;
  if (ec)
    return EXIT_FAILURE;

  using transferase::counts_file_format;
  using transferase::genome_index;
  using transferase::get_meth_file_format;
  using transferase::log_level_t;
  using transferase::logger;
  using transferase::methylome;
  using transferase::methylome_metadata;

  auto &lgr =
    logger::instance(transferase::shared_from_cout(), command, args.log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  args.methylome_name = methylome::parse_methylome_name(args.methylation_input);

  if (args.index_directory.empty()) {
    args.index_directory =
      transferase::client_config::get_index_dir_default(ec);
    if (ec) {
      lgr.error("Failure identifying index directory: {}", ec.message());
      return EXIT_FAILURE;
    }
  }

  args.log_options();

  std::error_code index_ec;
  const auto index =
    genome_index::read(args.index_directory, args.genome_name, index_ec);
  if (index_ec) {
    lgr.error("Failed to read genome index {} {}: {}", args.index_directory,
              args.genome_name, index_ec);
    return EXIT_FAILURE;
  }

  const auto [format_id, format_err] =
    get_meth_file_format(args.methylation_input);
  if (format_err || format_id == counts_file_format::none) {
    lgr.error("Failed to identify file type for: {}", args.methylation_input);
    return EXIT_FAILURE;
  }
  lgr.info("Input file format: {}", message(format_id));

  auto [meth_data, meth_data_err] =
    (format_id == counts_file_format::xcounts)
      ? process_cpg_sites_xcounts(args.methylation_input, index)
      : process_cpg_sites_counts(args.methylation_input, index);

  if (meth_data_err) {
    lgr.error("Error generating methylome: {}", meth_data_err);
    return EXIT_FAILURE;
  }

  methylome meth{std::move(meth_data), methylome_metadata{}};

  const auto meth_init_err = meth.init_metadata(index);
  if (meth_init_err) {
    lgr.error("Error initializing methylome metadata: {}", meth_init_err);
    return EXIT_FAILURE;
  }

  // ADS: this is where compression status is determined, and then
  // effected as data is written
  meth.meta.is_compressed = args.zip;

  std::error_code write_err;
  meth.write(args.methylome_outdir, args.methylome_name, write_err);
  if (write_err) {
    lgr.error("Error writing methylome {} {}: {}", args.methylome_outdir,
              args.methylome_name, write_err);
    return EXIT_FAILURE;
  }

  const auto command_stop = std::chrono::high_resolution_clock::now();
  lgr.debug("Total methylome format time: {:.3}s",
            duration(command_start, command_stop));

  return EXIT_SUCCESS;
}
