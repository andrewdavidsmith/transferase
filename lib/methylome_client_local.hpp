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

#ifndef LIB_METHYLOME_CLIENT_LOCAL_HPP_
#define LIB_METHYLOME_CLIENT_LOCAL_HPP_

#include "methylome_client_base.hpp"

#include "client_config.hpp"
#include "genome_index.hpp"
#include "genome_index_set.hpp"
#include "level_container_md.hpp"
#include "methylome.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "request_type_code.hpp"

#include "nlohmann/json.hpp"

#include <cassert>
#include <cstdint>
#include <format>    // for std::vector??
#include <iterator>  // for std::size
#include <memory>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>  // for std::tuple
#include <vector>

// forward declarations
namespace transferase {
struct genomic_interval;
struct level_element_covered_t;
}  // namespace transferase

namespace transferase {

class methylome_client_local
  : public methylome_client_base<methylome_client_local> {
public:
  typedef methylome_client_base<methylome_client_local>
    methylome_client_local_parent;

  explicit methylome_client_local(const std::string &config_dir) :
    methylome_client_base(config_dir) {
    std::error_code error;
    validate_derived(error);
    if (error)
      throw std::system_error(error, "[Failed in local constructor]");
  }

  [[nodiscard]] auto
  tostring_derived() const noexcept -> std::string;

  auto
  validate_derived(std::error_code &error) noexcept -> void;

  // intervals: takes a query
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_derived(const std::vector<std::string> &methylome_names,
                     const query_container &query, std::error_code &error)
    const noexcept -> level_container_md<lvl_elem_t> {
    request_type_code req_type = request_type_code::intervals;
    if constexpr (std::is_same_v<lvl_elem_t, level_element_covered_t>)
      req_type = request_type_code::intervals_covered;
    const auto [_, index_hash] =
      get_genome_and_index_hash(methylome_names, error);
    if (error)
      return {};
    const auto req =
      request{req_type, index_hash, std::size(query), methylome_names};
    return get_levels_impl<lvl_elem_t>(req, query, error);
  }

  // intervals: takes a vector of genomic intervals
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_derived(const std::vector<std::string> &methylome_names,
                     const std::vector<genomic_interval> &intervals,
                     std::error_code &error) const noexcept
    -> level_container_md<lvl_elem_t> {
    request_type_code req_type = request_type_code::intervals;
    if constexpr (std::is_same_v<lvl_elem_t, level_element_covered_t>)
      req_type = request_type_code::intervals_covered;
    const auto [genome_name, index_hash] =
      get_genome_and_index_hash(methylome_names, error);
    if (error)
      return {};
    const auto index = indexes->get_genome_index(genome_name, error);
    if (error)
      return {};
    const auto query = index->make_query(intervals);
    const auto req =
      request{req_type, index_hash, std::size(query), methylome_names};
    return get_levels_impl<lvl_elem_t>(req, query, error);
  }

  // bins: takes an index
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_derived(const std::vector<std::string> &methylome_names,
                     const std::uint32_t bin_size, std::error_code &error)
    const noexcept -> level_container_md<lvl_elem_t> {
    request_type_code req_type = request_type_code::bins;
    if constexpr (std::is_same_v<lvl_elem_t, level_element_covered_t>)
      req_type = request_type_code::bins_covered;
    const auto [genome_name, index_hash] =
      get_genome_and_index_hash(methylome_names, error);
    if (error)
      return {};
    const auto index = indexes->get_genome_index(genome_name, error);
    if (error)
      return {};
    const auto req = request{req_type, index_hash, bin_size, methylome_names};
    return get_levels_impl<lvl_elem_t>(req, *index, error);
  }

private:
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const request &req, const query_container &query,
                  std::error_code &error) const noexcept
    -> level_container_md<lvl_elem_t> {
    assert(req.n_intervals() == std::size(query));
    level_container_md<lvl_elem_t> results(req.n_intervals(),
                                           req.n_methylomes());
    std::uint32_t col_id = 0;
    for (const auto &methylome_name : req.methylome_names) {
      const auto meth =
        methylome::read(config.get_methylome_dir(), methylome_name, error);
      if (error)
        return {};
      meth.get_levels<lvl_elem_t>(query, results.column_itr(col_id++));
    }
    return results;
  }

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const request &req, const genome_index &index,
                  std::error_code &error) const noexcept
    -> level_container_md<lvl_elem_t> {
    level_container_md<lvl_elem_t> results(index.get_n_bins(req.bin_size()),
                                           req.n_methylomes());
    std::uint32_t col_id = 0;
    for (const auto &methylome_name : req.methylome_names) {
      const auto meth =
        methylome::read(config.get_methylome_dir(), methylome_name, error);
      if (error)
        return {};
      meth.get_levels<lvl_elem_t>(req.bin_size(), index,
                                  results.column_itr(col_id++));
    }
    return results;
  }

  [[nodiscard]] auto
  get_genome_and_index_hash(const std::vector<std::string> &methylome_names,
                            std::error_code &error) const noexcept
    -> std::tuple<std::string, std::uint64_t>;

public:
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(methylome_client_local, config)
};

}  // namespace transferase

/// @brief Enum for error codes related to methylome_client_local
enum class methylome_client_local_error_code : std::uint8_t {
  ok = 0,
  error_reading_config_file = 1,
  required_config_values_not_found = 2,
  methylome_dir_not_found_in_config = 3,
  inconsistent_methylome_metadata = 4,
};

template <>
struct std::is_error_code_enum<methylome_client_local_error_code>
  : public std::true_type {};

struct methylome_client_local_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "methylome_client_local"; }
  auto message(int code) const noexcept -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error reading default config file"s;
    case 2: return "required config values not found"s;
    case 3: return "methylome dir not found in config"s;
    case 4: return "inconsistent methylome metadata"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(methylome_client_local_error_code e) noexcept
  -> std::error_code {
  static auto category = methylome_client_local_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // LIB_METHYLOME_CLIENT_LOCAL_HPP_
