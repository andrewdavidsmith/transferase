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

#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_metadata.hpp"
#include "mxe_error.hpp"
#include "utilities.hpp"

#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

#include <boost/program_options.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <iostream>
#include <limits>
#include <numeric>
#include <print>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using std::println;
using std::size;
using std::string;

enum class format_err {
  // clang-format off
  ok                                         = 0,
  xcounts_file_open_failure                  = 1,
  xcounts_file_header_failure                = 2,
  xcounts_file_chromosome_not_found          = 3,
  xcounts_file_inconsistent_chromosome_order = 4,
  xcounts_file_incorrect_chromosome_size     = 5,
  methylome_format_failure                   = 6,
  methylome_file_write_failure               = 7,
  // clang-format on
};

struct format_err_cat : std::error_category {
  auto name() const noexcept -> const char * override { return "format error"; }
  auto message(const int condition) const -> std::string override {
    using std::string_literals::operator""s;
    // clang-format off
    switch (condition) {
    case 0: return "ok"s;
    case 1: return "failed to open methylome file"s;
    case 2: return "failed to parse xcounts header"s;
    case 3: return "failed to find chromosome in xcounts header"s;
    case 4: return "inconsistent chromosome order"s;
    case 5: return "incorrect chromosome size"s;
    case 6: return "failed to generate methylome file"s;
    case 7: return "failed to write methylome file"s;
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

struct meth_file {
  [[nodiscard]] auto open(const std::string &filename) -> std::error_code {
    in = gzopen(filename.data(), "rb");
    if (in == nullptr)
      return format_err::xcounts_file_open_failure;
    return format_err::ok;
  }

  [[nodiscard]] auto read() -> int {
    len = gzread(in, buf.data(), buf_size);
    pos = 0;
    return len;
  }

  [[nodiscard]] auto getline(std::string &line) -> bool {
    line.clear();
    if (pos == len && !read())
      return false;
    while (buf[pos] != '\n') {
      line += buf[pos++];
      if (pos == len && !read())
        return false;
    }
    ++pos;  // if here, then buf[pos] == '\n'
    return true;
  }

  static constexpr std::int32_t buf_size = 4 * 128 * 1024;
  gzFile in{};
  std::int32_t pos{};
  std::int32_t len{};
  std::array<std::uint8_t, buf_size> buf;
};

static inline auto
skip_absent_cpgs(const std::uint64_t end_pos, const cpg_index::vec &idx,
                 std::uint32_t &cpg_idx_in) -> std::uint32_t {
  const std::uint32_t first_idx = cpg_idx_in;
  while (cpg_idx_in < size(idx) && idx[cpg_idx_in] < end_pos)
    ++cpg_idx_in;
  return cpg_idx_in - first_idx;
}

// add all cpg sites for chroms in the given range
static inline auto
add_all_cpgs(const std::uint32_t prev_ch_id, const std::uint32_t ch_id,
             const cpg_index &idx) -> std::uint32_t {
  auto n_cpgs_total = 0;
  for (auto i = prev_ch_id + 1; i < ch_id; ++i)
    n_cpgs_total += size(idx.positions[i]);
  return n_cpgs_total;
}

static inline auto
get_ch_id(const cpg_index &ci, const std::string &chrom_name) -> std::int32_t {
  const auto ch_id = ci.chrom_index.find(chrom_name);
  if (ch_id == cend(ci.chrom_index))
    return -1;
  return ch_id->second;
}

// ADS: this function is tied to the specifics of dnmtools::xcounts
// code, and likely needs a review
static auto
verify_header_line(const cpg_index &idx, std::int32_t &n_chroms_seen,
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
  // methylome mxe file
  const auto order_itr = idx.chrom_index.find(chrom);
  if (order_itr == cend(idx.chrom_index))
    return format_err::xcounts_file_chromosome_not_found;

  if (n_chroms_seen != order_itr->second)
    return format_err::xcounts_file_inconsistent_chromosome_order;

  // validate that the chromosome size is the same between the index
  // and the methylome mxe file
  const auto size_itr = idx.chrom_size[order_itr->second];
  if (chrom_size != size_itr)
    return format_err::xcounts_file_incorrect_chromosome_size;

  ++n_chroms_seen;  // increment the number of chroms seen in the
                    // methylome file header

  return format_err::ok;
}

static auto
process_cpg_sites(const std::string &infile, const std::string &outfile,
                  const cpg_index &index, const bool zip) -> std::error_code {
  meth_file mf{};
  std::error_code err = mf.open(infile);
  if (err != format_err::ok) {
    std::println("Error reading xcounts file: {}", infile);
    return err;
  }

  methylome::vec cpgs(index.n_cpgs_total, {0, 0});

  std::uint32_t cpg_idx_out{};  // index of current output cpg position
  std::uint32_t cpg_idx_in{};   // index of current input cpg position
  std::int32_t prev_ch_id = -1;
  std::uint64_t pos = std::numeric_limits<std::uint64_t>::max();

  std::vector<cpg_index::vec>::const_iterator positions{};

  std::string line;
  std::int32_t n_chroms_seen{};
  while (mf.getline(line)) {
    if (line[0] == '#') {
      // consistency check between reference used for the index and
      // reference used for the methylome
      if (err = verify_header_line(index, n_chroms_seen, line); err) {
        std::println("Error parsing xcounts header line: {} ({})", line, err);
        return err;
      }
      continue;  // ADS: early loop exit
    }
    if (!std::isdigit(line[0])) {  // check for new chromosome
      // check not first iteration
      if (pos != std::numeric_limits<std::uint64_t>::max())
        // add cpgs before those in the input
        cpg_idx_out += (size(*positions) - cpg_idx_in);

      const std::int32_t ch_id = get_ch_id(index, line);
      if (ch_id < 0) {
        std::println("Failed to find chromosome in index: {}", line);
        return format_err::xcounts_file_chromosome_not_found;
      }

      cpg_idx_out += add_all_cpgs(prev_ch_id, ch_id, index);

      positions = cbegin(index.positions) + ch_id;
      pos = 0;         // position in genome
      cpg_idx_in = 0;  // index of current cpg site
      prev_ch_id = ch_id;
    }
    else {
      std::uint32_t pos_step = 0, n_meth = 0, n_unmeth = 0;
      const auto end_line = line.data() + size(line);
      auto res = std::from_chars(line.data(), end_line, pos_step);
      res = std::from_chars(res.ptr + 1, end_line, n_meth);
      res = std::from_chars(res.ptr + 1, end_line, n_unmeth);
      // ADS: check for errors here

      const auto curr_pos = pos + pos_step;
      if (pos + 1 < curr_pos)
        cpg_idx_out += skip_absent_cpgs(curr_pos, *positions, cpg_idx_in);

      // ADS: prevent counts from overflowing
      conditional_round_to_fit<methylome::m_count_t>(n_meth, n_unmeth);

      cpgs[cpg_idx_out++] = {static_cast<methylome::m_count_t>(n_meth),
                             static_cast<methylome::m_count_t>(n_unmeth)};
      pos = curr_pos;
      ++cpg_idx_in;
    }
  }
  cpg_idx_out += size(*positions) - cpg_idx_in;
  cpg_idx_out += add_all_cpgs(prev_ch_id, size(index.positions), index);

  if (cpg_idx_out != index.n_cpgs_total) {
    logger::instance().error(
      "Inconsistent numbers of cpgs (index={}, processed={})",
      index.n_cpgs_total, cpg_idx_out);
    return format_err::methylome_format_failure;
  }

  methylome m;
  m.cpgs = std::move(cpgs);
  const auto meth_write_err = m.write(outfile, zip);
  if (meth_write_err) {
    logger::instance().error("Error writing methylome {}: {}", outfile,
                             meth_write_err);
    return format_err::methylome_file_write_failure;
  }

  assert(err == format_err::ok);

  return err;
}

auto
command_format_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "format";

  std::string methylation_input{};
  std::string methylome_output{};
  std::string metadata_output{};
  std::string index_file{};
  mxe_log_level log_level{};
  bool zip{false};

  namespace po = boost::program_options;

  po::options_description desc(std::format("Usage: mxe {} [options]", command));
  desc.add_options()
    // clang-format off
    ("help,h", "produce help message")
    ("meth,m", po::value(&methylation_input)->required(),
     "methylation input file")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("output,o", po::value(&methylome_output)->required(),
     "methylome output file")
    ("meta-out", po::value(&metadata_output),
     "metadata output (defaults to output.json)")
    ("zip,z", po::bool_switch(&zip), "zip the output")
    ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
     "log level {debug,info,warning,error,critical}")
    // clang-format on
    ;
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help") || argc == 1) {
      desc.print(std::cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    desc.print(std::cout);
    return EXIT_FAILURE;
  }

  auto &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  if (metadata_output.empty())
    metadata_output = std::format("{}.json", methylome_output);

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Methylation", methylation_input},
    {"Index", index_file},
    {"Methylome output", methylome_output},
    {"Metadata output", metadata_output},
    {"Zip", std::format("{}", zip)},
    // clang-format on
  };
  log_args<mxe_log_level::info>(args_to_log);

  cpg_index index{};
  if (const auto index_read_err = index.read(index_file); index_read_err) {
    lgr.error("Error reading cpg index {}: {}", index_read_err, index_file);
    return EXIT_FAILURE;
  }

  std::error_code err =
    process_cpg_sites(methylation_input, methylome_output, index, zip);
  if (err) {
    lgr.error("Error generating methylome: {}", err);
    return EXIT_FAILURE;
  }

  const auto [metadata, metadata_err] =
    methylome_metadata::init(index_file, methylome_output, zip);
  if (metadata_err) {
    lgr.error("Error initializing metadata: {}", metadata_err);
    return EXIT_FAILURE;
  }

  err = methylome_metadata::write(metadata, metadata_output);
  if (err) {
    lgr.error("Error writing metadata: {} ({})", err, metadata_output);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
