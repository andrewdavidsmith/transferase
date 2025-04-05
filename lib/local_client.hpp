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

#ifndef LIB_LOCAL_CLIENT_HPP_
#define LIB_LOCAL_CLIENT_HPP_

#include "client_base.hpp"

#include "client_config.hpp"
#include "genome_index.hpp"
#include "genome_index_set.hpp"
#include "level_container_md.hpp"
#include "methylome.hpp"
#include "query_container.hpp"

#include <cstdint>
#include <format>    // for std::vector??
#include <iterator>  // for std::size
#include <memory>
#include <string>
#include <system_error>
#include <vector>

// forward declarations
namespace transferase {
struct genomic_interval;
}  // namespace transferase

namespace transferase {

class local_client : public client_base<local_client> {
public:
  explicit local_client(const std::string &config_dir) :
    client_base(config_dir) {
    std::error_code error;
    validate(error);
    if (error)
      throw std::system_error(error, "[Failed in local constructor]");
  }

  auto
  validate(std::error_code &error) noexcept -> void;

  // intervals: takes a query
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::vector<std::string> &methylome_names,
             const query_container &query) const
    -> level_container_md<lvl_elem_t> {
    std::error_code error{};
    auto result = get_levels_impl<lvl_elem_t>(methylome_names, query, error);
    if (error)
      throw std::system_error(error);
    return result;
  }

  // intervals: takes a vector of genomic intervals
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::vector<std::string> &methylome_names,
             const std::vector<genomic_interval> &intervals) const
    -> level_container_md<lvl_elem_t> {
    std::error_code error{};
    const auto [genome_name, _] = methylome::get_genome_info(
      config.get_methylome_dir(), methylome_names.at(0), error);
    if (error)
      throw std::system_error(error);
    const auto index = indexes->get_genome_index(genome_name, error);
    if (error)
      throw std::system_error(error);
    const auto query = index->make_query(intervals);
    auto result = get_levels_impl<lvl_elem_t>(methylome_names, query, error);
    if (error)
      throw std::system_error(error);
    return result;
  }

  // bins: takes an index
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::vector<std::string> &methylome_names,
             const std::uint32_t bin_size) const
    -> level_container_md<lvl_elem_t> {
    std::error_code error{};
    const auto [genome_name, _] = methylome::get_genome_info(
      config.get_methylome_dir(), methylome_names.at(0), error);
    if (error)
      return {};
    const auto index = indexes->get_genome_index(genome_name, error);
    if (error)
      return {};
    auto result =
      get_levels_impl<lvl_elem_t>(methylome_names, *index, bin_size, error);
    if (error)
      throw std::system_error(error);
    return result;
  }

private:
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const std::vector<std::string> &methylome_names,
                  const query_container &query, std::error_code &error)
    const noexcept -> level_container_md<lvl_elem_t> {
    level_container_md<lvl_elem_t> results(std::size(query),
                                           std::size(methylome_names));
    bool first_methylome = true;
    std::uint64_t index_hash = 0;
    std::uint32_t col_id = 0;
    for (const auto &methylome_name : methylome_names) {
      const auto meth =
        methylome::read(config.get_methylome_dir(), methylome_name, error);
      if (error)
        return {};
      if (first_methylome) {
        index_hash = meth.get_index_hash();
        first_methylome = false;
      }
      else if (index_hash != meth.get_index_hash()) {
        error = client_error_code::inconsistent_methylome_metadata;
        return {};
      }
      meth.get_levels<lvl_elem_t>(query, results.column_itr(col_id++));
    }
    return results;
  }

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const std::vector<std::string> &methylome_names,
                  const genome_index &index, const std::uint32_t bin_size,
                  std::error_code &error) const noexcept
    -> level_container_md<lvl_elem_t> {
    level_container_md<lvl_elem_t> results(index.get_n_bins(bin_size),
                                           std::size(methylome_names));
    bool first_methylome = true;
    std::uint64_t index_hash = 0;
    std::uint32_t col_id = 0;
    for (const auto &methylome_name : methylome_names) {
      const auto meth =
        methylome::read(config.get_methylome_dir(), methylome_name, error);
      if (error)
        return {};
      if (first_methylome) {
        index_hash = meth.get_index_hash();
        first_methylome = false;
      }
      else if (index_hash != meth.get_index_hash()) {
        error = client_error_code::inconsistent_methylome_metadata;
        return {};
      }
      meth.get_levels<lvl_elem_t>(bin_size, index,
                                  results.column_itr(col_id++));
    }
    return results;
  }
};

}  // namespace transferase

#endif  // LIB_LOCAL_CLIENT_HPP_
