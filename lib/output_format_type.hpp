/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#ifndef LIB_OUTPUT_FORMAT_TYPE_HPP_
#define LIB_OUTPUT_FORMAT_TYPE_HPP_

#include <array>
#include <cstdint>
#include <format>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <utility>  // for std::to_underlying

// NOLINTNEXTLINE
static const auto output_format_details = R"(
The output formats for the 'query' command are the following:

* classic
* counts
* scores
* dfclassic
* dfcounts
* dfscores

Output formats are broken into 2 sets this way:

Data frames: Those that begin with 'df' will be data frames (as used in R),
with column names (a header) for all but the first column, and the row names
as the first column.

Non-data frames: These have no column headers. The columns are ordered
according to the order given for query methylomes, and then follow the rules
below. Each row includes the 3-column BED interval as the first 3 columns of
the output.

Output formats are broken into 3 sets this way:

Classic: For each methylome involved in a query, there are at least two output
columns. (1) The score: a fraction equal to n_meth/(n_meth + n_unmeth), where
n_meth is the number of methylated observations in sequenced reads, and
n_unmeth is similarly the number of unmethylated observations. (2) The read
count, which is (n_meth + n_unmeth). If the output is a data frame, then the
score column name will have the suffix "_M" and the read counts column name
have the suffix "_R". If the query requests the number of covered sites per
interval per methylome, then this information will be in a third column,
having the suffix "_C". The advantage of this format is that the fractions are
immediately available. The disadvantage is that these files can be larger, and
take longer to read and write.

Counts: For each methylome involved in a query, there are at least two output
columns.  (1) The count 'n_meth' of methylated reads, and (2) the count
'n_unmeth' of unmethylated reads. Clearly these two counts can be used to
obtain the same information as in the 'classic' format. The advantage of this
format is that it's smaller, so disk read and writes are faster. The
disadvantage is that if you want the 'score' you need to do the division
yourself. If the output is in data frame format, then the n_meth column has a
"_M" suffix, and the n_unmeth column has a "_U" suffix. If you request sites
covered, then a third column will be present, as explained for 'classic'
above.

Scores: This format is similar to 'classic' except it does not include the
read count column. This makes the format slightly smaller, and more convenient
to work with (e.g., you can do PCA in R with very few steps).  However, any
query interval for which there are no reads will have an undefined value, and
without the read count column, you wouldn't be able to distinguish this
situation from a very confident low methylation level. The `--min-reads`
parameter will ensure that a value of "NA" is used when the number of reads
fails to meet your cutoff for confidence. If you only have one methylome in
your query, don't request a data frame, and let `--min-reads` be zero, then
you have a bedgraph.

So the possible output formats are the 6 combinations of
{data-frame, not-data-frame} x {classic, counts, scores}

)";

namespace transferase {

enum class output_format_t : std::uint8_t {
  counts = 0,
  classic = 1,
  scores = 2,
  dfcounts = 3,
  dfclassic = 4,
  dfscores = 5,
};

using std::literals::string_view_literals::operator""sv;
static constexpr auto output_format_t_name = std::array{
  // clang-format off
  "counts"sv,
  "classic"sv,
  "scores"sv,
  "dfcounts"sv,
  "dfclassic"sv,
  "dfscores"sv,
  // clang-format on
};

static constexpr auto output_format_help_str =
  "{counts, classic, scores, dfcounts, dfclassic, dfscores}";

static const std::map<std::string, output_format_t> output_format_cli11{
  {"counts", output_format_t::counts},
  {"classic", output_format_t::classic},
  {"scores", output_format_t::scores},
  {"dfcounts", output_format_t::dfcounts},
  {"dfclassic", output_format_t::dfclassic},
  {"dfscores", output_format_t::dfscores},
};

auto
operator<<(std::ostream &o, const output_format_t &of) -> std::ostream &;

auto
operator>>(std::istream &in, output_format_t &of) -> std::istream &;

}  // namespace transferase

template <>
struct std::formatter<transferase::output_format_t>
  : std::formatter<std::string> {
  auto
  format(const transferase::output_format_t &x, auto &ctx) const {
    const auto u = std::to_underlying(x);
    const auto v = transferase::output_format_t_name[u];
    return std::formatter<std::string>::format(
      std::string(std::cbegin(v), std::cend(v)), ctx);
  }
};

#endif  // LIB_OUTPUT_FORMAT_TYPE_HPP_
