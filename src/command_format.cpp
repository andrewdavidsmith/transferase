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
json_pp). These two files should reside in the same directory and
typically only the methylome data file is specified when it is used.
If xfrase is used remotely, the methylome will reside on the server.  If
you are analyzing your own DNA methylation data, you will need to
format your methylomes with this command.
)";

static constexpr auto examples = R"(
Examples:

xfrase format -x hg38.cpg_idx -m SRX012345.xsym.gz -o SRX012345.m16
)";

#include "counts_file_formats.hpp"
#include "cpg_index.hpp"
#include "cpg_index_metadata.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_metadata.hpp"
#include "utilities.hpp"
#include "xfrase_error.hpp"  // IWYU pragma: keep
#include "zlib_adapter.hpp"

#include <boost/program_options.hpp>

#include <cctype>  // for std::isdigit
#include <charconv>
#include <chrono>
#include <cstdint>  // for std::uint32_t, std::uint64_t, std::int32_t
#include <cstdlib>  // for EXIT_FAILURE, abort, EXIT_SUCCESS
#include <filesystem>
#include <format>
#include <iostream>
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

enum class format_err {
  // clang-format off
  ok                                         = 0,
  xcounts_file_open_failure                  = 1,
  xcounts_file_header_failure                = 2,
  xcounts_file_chromosome_not_found          = 3,
  xcounts_file_incorrect_chromosome_size     = 4,
  methylome_format_failure                   = 5,
  methylome_file_write_failure               = 6,
  // clang-format on
};

struct format_err_cat : std::error_category {
  auto
  name() const noexcept -> const char * override {
    return "format error";
  }
  auto
  message(const int condition) const -> std::string override {
    using std::string_literals::operator""s;
    // clang-format off
    switch (condition) {
    case 0: return "ok"s;
    case 1: return "failed to open methylome file"s;
    case 2: return "failed to parse xcounts header"s;
    case 3: return "failed to find chromosome in xcounts header"s;
    case 4: return "incorrect chromosome size"s;
    case 5: return "failed to generate methylome file"s;
    case 6: return "failed to write methylome file"s;
    }
    // clang-format on
    std::abort();  // unreacheable
  }
};

template <>
struct std::is_error_code_enum<format_err> : public std::true_type {};

std::error_code
make_error_code(format_err e) {
  static auto category = format_err_cat{};
  return std::error_code(std::to_underlying(e), category);
}

static inline auto
skip_absent_cpgs(const std::uint64_t end_pos, const cpg_index::vec &idx,
                 std::uint32_t cpg_idx_in) -> std::uint32_t {
  const std::uint32_t first_idx = cpg_idx_in;
  while (cpg_idx_in < size(idx) && idx[cpg_idx_in] < end_pos)
    ++cpg_idx_in;
  return cpg_idx_in - first_idx;
}

static inline auto
get_ch_id(const cpg_index_metadata &cim,
          const std::string &chrom_name) -> std::int32_t {
  const auto ch_id = cim.chrom_index.find(chrom_name);
  if (ch_id == cend(cim.chrom_index))
    return -1;
  return ch_id->second;
}

// ADS: this function is tied to the specifics of dnmtools::xcounts
// code, and likely needs a review
static auto
verify_header_line(const cpg_index_metadata &cim,
                   const std::string &line) -> std::error_code {
  // ignore the version line and the header end line
  if (line.substr(0, 9) == "#DNMTOOLS" || size(line) == 1)
    return format_err::ok;

  // parse the chrom and its size
  std::string chrom;
  std::uint64_t chrom_size{};
  std::istringstream iss{line};
  if (!(iss >> chrom >> chrom_size))
    return format_err::xcounts_file_header_failure;

  chrom = chrom.substr(1);  // remove leading '#'

  // validate the chromosome order is consistent between the index and
  // methylome xfrase file
  const auto order_itr = cim.chrom_index.find(chrom);
  if (order_itr == cend(cim.chrom_index))
    return format_err::xcounts_file_chromosome_not_found;

  // validate that the chromosome size is the same between the index
  // and the methylome xfrase file
  const auto size_itr = cim.chrom_size[order_itr->second];
  if (chrom_size != size_itr)
    return format_err::xcounts_file_incorrect_chromosome_size;

  return format_err::ok;
}

static auto
process_cpg_sites(const std::string &infile, const cpg_index &index,
                  const cpg_index_metadata &cim)
  -> std::tuple<methylome, std::error_code> {
  auto &lgr = logger::instance();

  std::error_code err;
  gzinfile mf(infile, err);
  if (err != zlib_adapter_error::ok) {
    lgr.error("Error reading xcounts file: {}", infile);
    return {{}, err};
  }

  std::uint32_t cpg_idx_in{};  // index of current input cpg position
  std::uint64_t pos = std::numeric_limits<std::uint64_t>::max();

  std::vector<cpg_index::vec>::const_iterator positions{};

  // ADS: if optimization is needed, this can be flattened here to
  // avoid a copy later
  std::vector<methylome::vec> cpgs;
  for (const auto n_cpgs_chrom : cim.get_n_cpgs_chrom())
    cpgs.push_back(methylome::vec(n_cpgs_chrom));
  std::uint32_t cpg_idx_out{};

  methylome::vec::iterator cpgs_itr;

  std::string line;
  while (mf.getline(line)) {
    if (line[0] == '#') {
      // consistency check between reference used for the index and
      // reference used for the methylome
      if (err = verify_header_line(cim, line); err) {
        lgr.error("Error parsing xcounts header line: {} ({})", line, err);
        return {{}, err};
      }
      continue;  // ADS: early loop exit
    }
    if (!std::isdigit(line[0])) {  // check for new chromosome
      const std::int32_t ch_id = get_ch_id(cim, line);
      if (ch_id < 0) {
        lgr.error("Failed to find chromosome in index: {}", line);
        return {methylome{}, format_err::xcounts_file_chromosome_not_found};
      }
      cpg_idx_out = 0;

      positions = std::cbegin(index.positions) + ch_id;
      pos = 0;         // position in genome
      cpg_idx_in = 0;  // index of current cpg site

      cpgs_itr = std::begin(cpgs[ch_id]);
    }
    else {
      std::uint32_t pos_step = 0, n_meth = 0, n_unmeth = 0;
      const auto end_line = line.data() + size(line);
      auto res = std::from_chars(line.data(), end_line, pos_step);
      res = std::from_chars(res.ptr + 1, end_line, n_meth);
      res = std::from_chars(res.ptr + 1, end_line, n_unmeth);
      // ADS: check for errors here

      const auto curr_pos = pos + pos_step;
      if (pos + 1 < curr_pos) {
        const auto n_skips = skip_absent_cpgs(curr_pos, *positions, cpg_idx_in);
        cpg_idx_out += n_skips;
        cpg_idx_in += n_skips;
      }

      // ADS: prevent counts from overflowing
      conditional_round_to_fit<methylome::m_count_t>(n_meth, n_unmeth);

      *(cpgs_itr + cpg_idx_out++) = {
        static_cast<methylome::m_count_t>(n_meth),
        static_cast<methylome::m_count_t>(n_unmeth),
      };

      pos = curr_pos;
      ++cpg_idx_in;
    }
  }

  methylome::vec cpgs_flat;
  cpgs_flat.reserve(cim.n_cpgs);
  for (const auto &c : cpgs)
    cpgs_flat.insert(std::end(cpgs_flat), std::cbegin(c), std::cend(c));

  // tuple is move-returned, but making the tuple would copy
  return {methylome{std::move(cpgs_flat)}, std::error_code{}};
}

static auto
process_cpg_sites_counts(const std::string &infile, const cpg_index &index,
                         const cpg_index_metadata &cim)
  -> std::tuple<methylome, std::error_code> {
  auto &lgr = logger::instance();

  std::error_code err;
  gzinfile mf(infile, err);
  if (err != zlib_adapter_error::ok) {
    lgr.error("Error reading counts file: {}", infile);
    return {{}, err};
  }

  std::uint32_t cpg_idx_in{};  // index of current input cpg position
  std::uint64_t pos = std::numeric_limits<std::uint64_t>::max();

  std::vector<cpg_index::vec>::const_iterator positions{};

  // ADS: if optimization is needed, this can be flattened here to
  // avoid a copy later
  std::vector<methylome::vec> cpgs;
  for (const auto n_cpgs_chrom : cim.get_n_cpgs_chrom())
    cpgs.push_back(methylome::vec(n_cpgs_chrom));
  std::uint32_t cpg_idx_out{};

  methylome::vec::iterator cpgs_itr;

  std::string prev_chrom;
  std::string line;
  bool parse_success{true};
  while (parse_success && mf.getline(line)) {
    if (line[0] == '#')
      continue;
    const auto end_of_chrom = line.find_first_of(" \t");
    const std::string chrom{line.substr(0, end_of_chrom)};
    if (chrom != prev_chrom) {  // check for new chromosome
      const std::int32_t ch_id = get_ch_id(cim, chrom);
      if (ch_id < 0) {
        lgr.error("Failed to find chromosome in index: {}", line);
        return {methylome{}, /* ADS: fix this */
                format_err::xcounts_file_chromosome_not_found};
      }
      cpg_idx_out = 0;

      positions = std::cbegin(index.positions) + ch_id;
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
    conditional_round_to_fit<methylome::m_count_t>(n_meth, n_unmeth);

    *(cpgs_itr + cpg_idx_out++) = {
      static_cast<methylome::m_count_t>(n_meth),
      static_cast<methylome::m_count_t>(n_unmeth),
    };

    pos = curr_pos;
    ++cpg_idx_in;
  }

  methylome::vec cpgs_flat;
  cpgs_flat.reserve(cim.n_cpgs);
  for (const auto &c : cpgs)
    cpgs_flat.insert(std::end(cpgs_flat), std::cbegin(c), std::cend(c));

  // tuple is move-returned, but making the tuple would copy
  return {methylome{std::move(cpgs_flat)}, std::error_code{}};
}

auto
command_format_main(int argc, char *argv[]) -> int {
  const auto command_start = std::chrono::high_resolution_clock::now();

  static constexpr auto command = "format";
  static const auto usage =
    std::format("Usage: xfrase {} [options]\n", strip(command));
  static const auto about_msg =
    std::format("xfrase {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  std::string methylation_input{};
  std::string methylome_output{};
  std::string index_file{};
  xfrase_log_level log_level{};
  bool zip{false};

  namespace po = boost::program_options;

  po::options_description desc("Options");
  desc.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("meth,m", po::value(&methylation_input)->required(),
     "methylation input file")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("output,o", po::value(&methylome_output)->required(),
     std::format("output file (must end in {})", methylome::filename_extension).data())
    ("zip,z", po::bool_switch(&zip), "zip the output")
    ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
     "log level {debug,info,warning,error,critical}")
    // clang-format on
    ;
  try {
    po::variables_map vm;
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

  auto &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  const auto output_check = check_output_file(methylome_output);
  if (output_check) {
    lgr.error("Methylome output file {}: {}", methylome_output, output_check);
    return EXIT_FAILURE;
  }

  const auto extension_found =
    std::filesystem::path(methylome_output).extension();
  if (extension_found != methylome::filename_extension) {
    lgr.error("Required filename extension {} (given: {})",
              methylome::filename_extension, extension_found);
    return EXIT_FAILURE;
  }

  const auto metadata_output =
    get_default_methylome_metadata_filename(methylome_output);

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Methylation", methylation_input},
    {"Index", index_file},
    {"Methylome output", methylome_output},
    {"Metadata output", metadata_output},
    {"Zip", std::format("{}", zip)},
    // clang-format on
  };
  log_args<xfrase_log_level::info>(args_to_log);

  const auto [index, cim, index_read_err] = read_cpg_index(index_file);
  if (index_read_err) {
    lgr.error("Failed to read cpg index {}: {}", index_file, index_read_err);
    return EXIT_FAILURE;
  }

  const auto [format_id, format_err] = get_meth_file_format(methylation_input);
  if (format_err || format_id == counts_format::none) {
    lgr.error("Failed to identify file type for: {}", methylation_input);
    return EXIT_FAILURE;
  }
  lgr.info("Input file format: {}", message(format_id));

  const auto [meth, meth_err] =
    (format_id == counts_format::xcounts)
      ? process_cpg_sites(methylation_input, index, cim)
      : process_cpg_sites_counts(methylation_input, index, cim);

  if (meth_err) {
    lgr.error("Error generating methylome: {}", meth_err);
    return EXIT_FAILURE;
  }

  const auto [meta, meta_err] = methylome_metadata::init(cim, meth, zip);
  if (meta_err) {
    lgr.error("Error initializing metadata: {}", meta_err);
    return EXIT_FAILURE;
  }

  if (const auto write_err = meth.write(methylome_output, zip); write_err) {
    lgr.error("Error writing methylome {}: {}", methylome_output, write_err);
    return EXIT_FAILURE;
  }

  if (const auto write_err = meta.write(metadata_output); write_err) {
    lgr.error("Error writing metadata {}: {}", metadata_output, write_err);
    return EXIT_FAILURE;
  }

  const auto command_stop = std::chrono::high_resolution_clock::now();
  lgr.debug("Total methylome format time: {:.3}s",
            duration(command_start, command_stop));

  return EXIT_SUCCESS;
}
