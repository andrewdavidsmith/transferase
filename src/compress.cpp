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

#include "compress.hpp"
#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "methylome.hpp"

#include <zlib.h>

#include <boost/program_options.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numeric>
#include <print>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

using std::from_chars;
using std::max;
using std::pair;
using std::println;
using std::size;
using std::string;
using std::unordered_map;
using std::vector;

namespace po = boost::program_options;
using po::value;

template <typename T> using num_lim = std::numeric_limits<T>;

struct meth_file {
  [[nodiscard]] auto
  open(const string &filename) -> int {
    in = gzopen(filename.data(), "rb");
    if (in == nullptr)
      return -1;
    return 0;
  }

  [[nodiscard]] auto
  read() -> int {
    len = gzread(in, buf.data(), buf_size);
    pos = 0;
    return len;
  }

  [[nodiscard]] auto
  getline(string &line) -> bool {
    line.clear();
    if (pos == len && !read())
      return false;
    while (buf[pos] != '\n') {
      line += buf[pos++];
      if (pos == len && !read())
        return false;
    }
    ++pos;  // here means buf[pos] == '\n'
    return true;
  }

  static constexpr int32_t buf_size = 4 * 128 * 1024;
  gzFile in{};
  int32_t pos{};
  int32_t len{};
  std::array<uint8_t, buf_size> buf;
};

static inline auto
skip_absent_cpgs(const uint64_t end_pos, const cpg_index::vec &idx,
                 uint32_t &cpg_idx_in) -> uint32_t {
  const uint32_t first_idx = cpg_idx_in;
  while (cpg_idx_in < size(idx) && idx[cpg_idx_in] < end_pos)
    ++cpg_idx_in;
  return cpg_idx_in - first_idx;
}

// add all cpg sites for chroms in the given range
static inline auto
add_all_cpgs(const uint32_t prev_ch_id, const uint32_t ch_id,
             const cpg_index &idx) -> uint32_t {
  auto n_cpgs_total = 0;
  for (auto i = prev_ch_id + 1; i < ch_id; ++i)
    n_cpgs_total += size(idx.positions[i]);
  return n_cpgs_total;
}

static inline auto
get_ch_id(const cpg_index &ci, const string &chrom_name) -> int32_t {
  const auto ch_id = ci.chrom_index.find(chrom_name);
  if (ch_id == cend(ci.chrom_index))
    return -1;
  return ch_id->second;
}

static auto
verify_header_line(const cpg_index &idx, int32_t &n_chroms_seen,
                   const string &line) -> bool {
  // ignore the version line and the header end line
  if (line.substr(0, 9) == "#DNMTOOLS" || size(line) == 1)
    return true;

  // parse the chrom and its size
  string chrom;
  uint64_t chrom_size{};
  std::istringstream iss{line};
  if (!(iss >> chrom >> chrom_size))
    return false;
  chrom = chrom.substr(1);  // remove leading '#'

  // validate the chromosome order is consistent between the index and
  // methylome mc16 file
  const auto order_itr = idx.chrom_index.find(chrom);
  if (order_itr == cend(idx.chrom_index))
    return false;
  if (n_chroms_seen != order_itr->second)
    return false;

  // validate that the chromosome size is the same between the index
  // and the methylome mc16 file
  const auto size_itr = idx.chrom_size[order_itr->second];
  if (chrom_size != size_itr)
    return false;

  ++n_chroms_seen;  // increment the number of chroms seen in the
                    // methylome file header
  return true;
}

template <typename T>
static inline auto
round_to_fit(uint32_t &n_meth, uint32_t &n_unmeth) -> void {
  const uint32_t m = max(n_unmeth, n_meth);
  n_meth = n_meth == m ? num_lim<T>::max()
                       : (n_meth / static_cast<double>(m)) * num_lim<T>::max();
  n_unmeth = n_unmeth == m
               ? num_lim<T>::max()
               : (n_unmeth / static_cast<double>(m)) * num_lim<T>::max();
}

static auto
process_cpg_sites(const string &infile, const string &outfile,
                  const cpg_index &index, const bool zip) -> int {
  meth_file mf{};
  const auto open_ret = mf.open(infile);
  if (open_ret != 0) {
    println("failed to open input file: {}", infile);
    return -1;
  }

  methylome::vec cpgs(index.n_cpgs_total, {0, 0});

  uint32_t cpg_idx_out{};  // index of current output cpg position
  uint32_t cpg_idx_in{};   // index of current input cpg position
  int32_t prev_ch_id = -1;
  uint64_t pos = num_lim<uint64_t>::max();

  vector<cpg_index::vec>::const_iterator positions;

  unordered_map<string, int32_t> header_chroms_index;

  string line;
  int32_t n_chroms_seen{};
  while (mf.getline(line)) {
    if (line[0] == '#') {
      // consistency check between reference used for the index and
      // reference used for the methylome
      if (!verify_header_line(index, n_chroms_seen, line)) {
        println("inconsistent chrom info: {}", line);
        return -1;
      }
      continue;  // ADS: early loop exit
    }
    if (!std::isdigit(line[0])) {           // check for new chromosome
      if (pos != num_lim<uint64_t>::max())  // check not first iteration
        // add cpgs before those in the input
        cpg_idx_out += (size(*positions) - cpg_idx_in);

      const int32_t ch_id = get_ch_id(index, line);
      if (ch_id < 0) {
        println("failed to find chromosome: {}", line);
        return -1;
      }

      cpg_idx_out += add_all_cpgs(prev_ch_id, ch_id, index);

      positions = cbegin(index.positions) + ch_id;
      pos = 0;         // position in genome
      cpg_idx_in = 0;  // index of current cpg site
      prev_ch_id = ch_id;
    }
    else {
      uint32_t pos_step = 0, n_meth = 0, n_unmeth = 0;
      const auto end_line = line.data() + size(line);
      auto res = from_chars(line.data(), end_line, pos_step);
      res = from_chars(res.ptr + 1, end_line, n_meth);
      res = from_chars(res.ptr + 1, end_line, n_unmeth);
      // ADS: check for errors here

      const auto curr_pos = pos + pos_step;
      if (pos + 1 < curr_pos)
        cpg_idx_out += skip_absent_cpgs(curr_pos, *positions, cpg_idx_in);

      // ADS: prevent counts from overflowing
      if (max(n_meth, n_unmeth) > num_lim<methylome::m_count_t>::max())
        round_to_fit<methylome::m_count_t>(n_meth, n_unmeth);

      cpgs[cpg_idx_out++] = {static_cast<methylome::m_count_t>(n_meth),
                             static_cast<methylome::m_count_t>(n_unmeth)};
      pos = curr_pos;
      ++cpg_idx_in;
    }
  }
  cpg_idx_out += size(*positions) - cpg_idx_in;
  cpg_idx_out += add_all_cpgs(prev_ch_id, size(index.positions), index);

  if (cpg_idx_out != index.n_cpgs_total) {
    println("failed to build entire compressed file: {}", outfile);
    return -1;
  }

  methylome m;
  m.cpgs = std::move(cpgs);
  return m.write(outfile, zip);
}

auto
compress_main(int argc, char *argv[]) -> int {
  static const auto description = "compress";

  bool verbose{};
  bool zip{};

  string methylation_input{};
  string methylation_output{};
  string index_file{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("meth,m", value(&methylation_input)->required(),
     "methylation input file")
    ("index,x", value(&index_file)->required(), "index file")
    ("output,o", value(&methylation_output)->required(),
     "methylation output file")
    ("zip,z", po::bool_switch(&zip), "zip the output")
    ("verbose,v", po::bool_switch(&verbose), "print more run info")
    ;
  // clang-format on
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
      desc.print(std::cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  } catch (po::error &e) {
    println("{}", e.what());
    desc.print(std::cout);
    return EXIT_FAILURE;
  }

  if (verbose)
    print("meth: {}\n"
          "index: {}\n"
          "output: {}\n"
          "zip: {}\n",
          methylation_input, methylation_output, index_file, zip);

  cpg_index index{};
  if (index.read(index_file) != 0) {
    println("failed to read index file: {}", index_file);
    return EXIT_FAILURE;
  }

  if (verbose)
    println("{}", index);

  return process_cpg_sites(methylation_input, methylation_output, index, zip);
}
