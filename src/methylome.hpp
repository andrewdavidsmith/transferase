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

#ifndef SRC_METHYLOME_HPP_
#define SRC_METHYLOME_HPP_

#include "level_container.hpp"
#include "level_element.hpp"
#include "methylome_data.hpp"
#include "methylome_metadata.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>  // for std::size_t
#include <cstdint>  // std::uint32_t
#include <format>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>  // for std::hash
#include <vector>

namespace transferase {

struct genome_index;
struct query_container;

/// Methylome is an abstraction for genome-wide methylation levels
/// measured at a subset of sites, in particular CpG sites. Each sites
/// is associated with two counts: the number of reads providing
/// methylated observations and the number providing unmethylated
/// observations -- so a pair of counts for each site. When stored on
/// disk a genomic_interval is in the for of two files: one a binary
/// data file and the other a JSON format metadata file. The binary
/// layout on disk directly corresponds to the data layout in objects
/// of the Methylome class. Important: the counts are 16-bit, which
/// would allow for up to 65535-fold coverage at any site. In
/// consturcting methylomes, if counts exceed this they are
/// proportionally reduced and rounded to integers such that the
/// largest of the two values is 65535. However, at present such a
/// data set would be prohibitively expensive to produce and would
/// incude substantial redundant information.
struct methylome {
  /// @brief Filename extension that identifies methylome data files
  static constexpr auto data_extn = methylome_data::filename_extension;

  /// @brief Filename extension that identifies methylome metadata files
  static constexpr auto meta_extn = methylome_metadata::filename_extension;

  /// @brief The counts of methylated and unmethylated observations at
  /// each site in the genome.
  methylome_data data;

  /// @brief A collection of metadata used to ensure this methylome is
  /// properly identified and associated with a genome index.
  methylome_metadata meta;

  /// @brief This default constructor is automatically generated by the
  /// compiler.
  methylome() = default;

  /// @brief Move constructor. Constructs a `methylome` object by
  /// moving the provided methylome_data and methylome_metadata
  /// objects.
  methylome(methylome_data &&data, methylome_metadata &&meta) :
    data{std::move(data)}, meta{std::move(meta)} {}

  /// @brief Explicitly deleted copy assignment operator. Copy assignment of
  /// `methylome` objects is not allowed.
  methylome &
  operator=(const methylome &) = delete;

  /// @brief Defaulted move constructor; `methylome` objects are
  /// movable.
  methylome(methylome &&) noexcept = default;

  /// @brief Defaulted move assignment operator; `methylome` objects
  /// are movable.
  methylome &
  operator=(methylome &&) noexcept = default;

  /// @brief Read a methylome object from a filesystem.
  /// @param directory Directory where the methylome files can be found.
  /// @param methylome_name Read the methylome for the sample or accession.
  /// @param error An error code that is set for any error while reading.
  /// @return A methylome object.
  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &methylome_name,
       std::error_code &error) noexcept -> methylome;

#ifndef TRANSFERASE_NOEXCEPT
  /// @brief Read a methylome object from a filesystem.
  /// @param directory Directory where the methylome files can be found.
  /// @param methylome_name Read the methylome for the sample or accession.
  /// @return A methylome object.
  /// @throw std::system_error if any error is encountered while reading.
  static auto
  read(const std::string &dirname,
       const std::string &methylome_name) -> methylome {
    std::error_code error;
    auto meth = read(dirname, methylome_name, error);
    if (error)
      throw std::system_error(error);
    return meth;
  }
#endif

  /// @brief Get the name of the genome corresponding to this methylome
  [[nodiscard]] auto
  get_genome_name() const noexcept -> const std::string & {
    return meta.genome_name;
  }

  /// @brief Get the index hash of the genome corresponding to this methylome
  [[nodiscard]] auto
  get_index_hash() const noexcept -> std::uint64_t {
    return meta.index_hash;
  }

  /// @brief Returns true iff a methylome is internally consistent
  [[nodiscard]] auto
  is_consistent() const noexcept -> bool {
    return meta.methylome_hash == data.hash();
  }

  /// @brief Returns true iff two methylomes are consistent with each
  /// other. This means they are the same size, and are based on the
  /// same reference genome.
  [[nodiscard]] auto
  is_consistent(const methylome &other) const noexcept -> bool {
    return meta.is_consistent(other.meta);
  }

  /// @brief Return the hash value for this methylome by examining the
  /// methylome_hash variable in the metadata.
  [[nodiscard]]
  auto
  get_hash() const noexcept -> std::uint64_t {
    return meta.methylome_hash;
  }

  /// @brief Write this methylome to the filesystem.
  /// @param directory The directory in where this methylome should be written.
  /// @param methylome_name The name of the methylome; determines filenames
  /// written.
  /// @param error An error code that is set if any error encountered while
  /// writing
  auto
  write(const std::string &outdir, const std::string &methylome_name,
        std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// @brief Write this methylome to the filesystem.
  /// @param directory The directory in where this methylome should be written.
  /// @param methylome_name The name of the methylome; determines filenames
  /// written.
  /// @throw std::system_error if any error is encountered while writing.
  auto
  write(const std::string &outdir, const std::string &name) const -> void {
    std::error_code error;
    write(outdir, name, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  /// @brief Initialize the metadata associated with this
  /// methylome. This information is used while constructing a
  /// methylome and is based on a genome_index data structure.
  [[nodiscard]] auto
  init_metadata(const genome_index &index) noexcept -> std::error_code;

  [[nodiscard]] auto
  update_metadata() noexcept -> std::error_code;

  /// @brief Generate a string representation of a methylome in JSON
  /// format
  [[nodiscard]] auto
  tostring() const noexcept -> std::string {
    return std::format(R"json({{"meta"={}, "data"={}}})json", meta.tostring(),
                       data.tostring());
  }

  /// @brief Add two methylomes together. WARNING: at present this
  /// could in theory overflow the counts if too many methylomes are added.
  auto
  add(const methylome &rhs) noexcept -> void {
    data.add(rhs.data);
  }

  /// @brief Get the global methylation level in the methylome based
  /// in the form of two integer counts of number of methylated and
  /// number of unmethylated read counts over the whole methylome.
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  global_levels() const noexcept -> lvl_elem_t {
    return data.global_levels<lvl_elem_t>();
  }

  /// @brief Convenience function that calls global_levels with
  /// specialized to return information about sites covered.
  [[nodiscard]] auto
  global_levels_covered() const noexcept -> level_element_covered_t {
    return data.global_levels<level_element_covered_t>();
  }

  /// @brief Get the methylation level for each interval represented in a given
  /// query container.
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const transferase::query_container &query) const
    -> level_container<lvl_elem_t> {
    return data.get_levels<lvl_elem_t>(query);
  }

  /// @brief Get the methylation level for each genomic 'bin' (i.e.,
  /// non-overlapping intervals of fixed size; not the same as
  /// windows, which may overlap).
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::uint32_t bin_size,
             const genome_index &index) const -> level_container<lvl_elem_t> {
    return data.get_levels<lvl_elem_t>(bin_size, index);
  }

  /// @brief Returns true iff the methylome files exist for the given name.
  /// @param directory The directory in which to look for files.
  /// @param methylome_name The name of the methylome to look for.
  [[nodiscard]] static auto
  files_exist(const std::string &directory,
              const std::string &methylome_name) noexcept -> bool;

  /// @brief List the names of methylomes that can be read from the given
  /// directory.
  /// @param directory The directory in which to look.
  /// @param error An error code set if any error is encountered while searching
  /// the directory.
  /// @return A vector of strings holding methylome names.
  [[nodiscard]] static auto
  list(const std::string &dirname,
       std::error_code &error) noexcept -> std::vector<std::string>;

#ifndef TRANSFERASE_NOEXCEPT
  /// @brief List the names of methylomes that can be read from the given
  /// directory.
  /// @param directory The directory in which to look.
  /// @return A vector of strings holding methylome names.
  /// @throw std::system_error if any error is encountered while searching
  /// the directory.
  [[nodiscard]] static auto
  list(const std::string &dirname) -> std::vector<std::string> {
    std::error_code error;
    auto methylome_names = list(dirname, error);
    if (error)
      throw std::system_error(error);
    return methylome_names;
  }
#endif

  /// @brief Parse a methylome name from a filename.
  [[nodiscard]] static auto
  parse_methylome_name(const std::string &filename) noexcept -> std::string;

  /// @brief Returns true iff a methylome name is valid (at present: contains
  /// only alphanumeric and underscore).
  [[nodiscard]] static auto
  is_valid_name(const std::string &methylome_name) noexcept -> bool {
    return std::ranges::all_of(
      methylome_name, [](const auto c) { return std::isalnum(c) || c == '_'; });
  }
};

}  // namespace transferase

// Specialization of std::hash for methylome.
template <> struct std::hash<transferase::methylome> {
  /// @brief Specializes std::hash for methylome by wrapping the
  /// methylome::get_hash function.
  auto
  operator()(const transferase::methylome &meth) const noexcept -> std::size_t {
    return meth.get_hash();
  }
};

/// @brief Enum for error codes related to methylome
enum class methylome_error_code : std::uint8_t {
  ok = 0,
  invalid_accession = 1,
  invalid_methylome_data = 2,
};

template <>
struct std::is_error_code_enum<methylome_error_code> : public std::true_type {};

struct methylome_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "methylome"; }
  auto message(int code) const noexcept -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "invalid accession"s;
    case 2: return "invalid methylome data"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(methylome_error_code e) noexcept -> std::error_code {
  static auto category = methylome_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_METHYLOME_HPP_
