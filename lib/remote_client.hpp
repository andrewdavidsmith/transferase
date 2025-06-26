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

#ifndef LIB_REMOTE_CLIENT_HPP_
#define LIB_REMOTE_CLIENT_HPP_

#include "client_base.hpp"
#include "client_config.hpp"
#include "client_connection.hpp"
#include "genome_index.hpp"
#include "genome_index_set.hpp"
#include "level_container.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "request_type_code.hpp"

#include <cstdint>
#include <format>
#include <iterator>  // for std::size
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

// forward declarations
namespace transferase {
struct genomic_interval;
struct level_element_covered_t;  // for is_same_v
}  // namespace transferase

namespace transferase {

class remote_client : public client_base<remote_client> {
public:
  explicit remote_client(const std::string &config_dir) :
    client_base(config_dir) {
    std::error_code error;
    validate(error);
    if (error)
      throw std::system_error(error, "[Failed in remote constructor]");
  }

  auto
  validate(std::error_code &error) noexcept -> void;

  // intervals: takes a query, genome name specified
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(
    const std::string &genome, const std::vector<std::string> &methylome_names,
    const query_container &query) const -> level_container<lvl_elem_t> {
    request_type_code req_type = request_type_code::intervals;
    if constexpr (std::is_same_v<lvl_elem_t, level_element_covered_t>)
      req_type = request_type_code::intervals_covered;
    std::error_code error{};
    const auto index = indexes->get_genome_index(genome, error);
    if (error)
      throw std::system_error(error);
    const auto req =
      request{req_type, index->get_hash(), std::size(query), methylome_names};
    auto result = get_levels_impl<lvl_elem_t>(req, query, error);
    if (error)
      throw std::system_error(error);
    return result;
  }

  // intervals: takes a vector of genomic intervals, genome name specified
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::string &genome,
             const std::vector<std::string> &methylome_names,
             const std::vector<genomic_interval> &intervals) const
    -> level_container<lvl_elem_t> {
    request_type_code req_type = request_type_code::intervals;
    if constexpr (std::is_same_v<lvl_elem_t, level_element_covered_t>)
      req_type = request_type_code::intervals_covered;
    std::error_code error{};
    const auto index = indexes->get_genome_index(genome, error);
    if (error)
      throw std::system_error(error);
    const auto query = index->make_query(intervals);
    const auto req =
      request{req_type, index->get_hash(), std::size(query), methylome_names};
    auto result = get_levels_impl<lvl_elem_t>(req, query, error);
    if (error)
      throw std::system_error(error);
    return result;
  }

  // bins: takes a bin size
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(
    const std::string &genome, const std::vector<std::string> &methylome_names,
    const std::uint32_t bin_size) const -> level_container<lvl_elem_t> {
    request_type_code req_type = request_type_code::bins;
    if constexpr (std::is_same_v<lvl_elem_t, level_element_covered_t>)
      req_type = request_type_code::bins_covered;
    std::error_code error{};
    const auto index = indexes->get_genome_index(genome, error);
    if (error)
      throw std::system_error(error);
    const auto req =
      request{req_type, index->get_hash(), bin_size, methylome_names};
    auto result = get_levels_impl<lvl_elem_t>(req, error);
    if (error)
      throw std::system_error(error);
    return result;
  }

  // windows: takes a window size and step
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(
    const std::string &genome, const std::vector<std::string> &methylome_names,
    const std::uint32_t window_size,
    const std::uint32_t window_step) const -> level_container<lvl_elem_t> {
    request_type_code req_type = request_type_code::windows;
    if constexpr (std::is_same_v<lvl_elem_t, level_element_covered_t>)
      req_type = request_type_code::windows_covered;
    std::error_code error{};
    const auto index = indexes->get_genome_index(genome, error);
    if (error)
      throw std::system_error(error);
    const auto aux_val = request::get_aux_for_windows(window_size, window_step);
    const auto req =
      request{req_type, index->get_hash(), aux_val, methylome_names};
    auto result = get_levels_impl<lvl_elem_t>(req, error);
    if (error)
      throw std::system_error(error);
    return result;
  }

private:
  // intervals: needs a query container
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const request &req, const query_container &query,
                  std::error_code &error) const noexcept
    -> level_container<lvl_elem_t> {
    intervals_client<lvl_elem_t> cl(config.hostname, config.port, req, query);
    error = cl.run();
    if (error)
      return {};
    return cl.take_levels();
  }

  // bins and windows: does not need a query container
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const request &req, std::error_code &error) const noexcept
    -> level_container<lvl_elem_t> {
    bins_client<lvl_elem_t> cl(config.hostname, config.port, req);
    error = cl.run();
    if (error)
      return {};
    return cl.take_levels();
  }
};

}  // namespace transferase

#endif  // LIB_REMOTE_CLIENT_HPP_
