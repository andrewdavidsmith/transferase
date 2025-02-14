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

#ifndef LIB_METHYLOME_CLIENT_REMOTE_HPP_
#define LIB_METHYLOME_CLIENT_REMOTE_HPP_

#include "client.hpp"
#include "client_config.hpp"
#include "methylome_client_base.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "request_type_code.hpp"

#include <boost/describe.hpp>

#include <cstdint>
#include <format>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

// forward declarations
namespace transferase {
struct level_element_covered_t;
template <typename level_element_type> struct level_container;
}  // namespace transferase

namespace transferase {

class methylome_client_remote
  : public methylome_client_base<methylome_client_remote> {
public:
  typedef methylome_client_base<methylome_client_remote>
    methylome_client_remote_parent;

  [[nodiscard]] auto
  tostring_derived() const noexcept -> std::string;

  auto
  validate_derived(std::error_code &error) noexcept -> void;

  // intervals: takes a query
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_derived(const std::vector<std::string> &methylome_names,
                     const query_container &query, std::error_code &error)
    const noexcept -> std::vector<level_container<lvl_elem_t>> {
    request_type_code req_type = request_type_code::intervals;
    if constexpr (std::is_same_v<lvl_elem_t, level_element_covered_t>)
      req_type = request_type_code::intervals_covered;
    const auto [_, index_hash] =
      get_genome_and_index_hash(methylome_names, error);
    if (error)
      return {};
    const auto req =
      request{req_type, index_hash, size(query), methylome_names};
    return get_levels_impl<lvl_elem_t>(req, query, error);
  }

  // bins: takes an index
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_derived(const std::vector<std::string> &methylome_names,
                     const std::uint32_t bin_size, std::error_code &error)
    const noexcept -> std::vector<level_container<lvl_elem_t>> {
    request_type_code req_type = request_type_code::bins;
    if constexpr (std::is_same_v<lvl_elem_t, level_element_covered_t>)
      req_type = request_type_code::bins_covered;
    const auto [_, index_hash] =
      get_genome_and_index_hash(methylome_names, error);
    if (error)
      return {};
    const auto req = request{req_type, index_hash, bin_size, methylome_names};
    return get_levels_impl<lvl_elem_t>(req, error);
  }

private:
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const request &req, const query_container &query,
                  std::error_code &error) const noexcept
    -> std::vector<level_container<lvl_elem_t>> {
    intervals_client<lvl_elem_t> cl(config.hostname, config.port, req, query);
    error = cl.run();
    if (error)
      return {};
    return cl.take_levels(error);
  }

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const request &req, std::error_code &error) const noexcept
    -> std::vector<level_container<lvl_elem_t>> {
    bins_client<lvl_elem_t> cl(config.hostname, config.port, req);
    error = cl.run();
    if (error)
      return {};
    return cl.take_levels(error);
  }

public:
  BOOST_DESCRIBE_CLASS(methylome_client_remote,
                       (methylome_client_remote_parent), (), (), ())
};

}  // namespace transferase

/// @brief Enum for error codes related to methylome_client_remote
enum class methylome_client_remote_error_code : std::uint8_t {
  ok = 0,
  error_reading_config_file = 1,
  required_config_values_not_found = 2,
  hostname_not_found = 3,
  port_not_found = 4,
  index_dir_not_found = 5,
  metadata_not_found = 6,
  failed_to_read_index_dir = 7,
  failed_to_read_metadata_file = 8,
};

template <>
struct std::is_error_code_enum<methylome_client_remote_error_code>
  : public std::true_type {};

struct methylome_client_remote_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "methylome_client_remote"; }
  auto message(int code) const noexcept -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error reading default config file"s;
    case 2: return "required config values not found"s;
    case 3: return "hostname not found"s;
    case 4: return "port not found"s;
    case 5: return "index dir not found"s;
    case 6: return "metadata not found"s;
    case 7: return "failed to read index dir"s;
    case 8: return "failed to read metadata file"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(methylome_client_remote_error_code e) noexcept
  -> std::error_code {
  static auto category = methylome_client_remote_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // LIB_METHYLOME_CLIENT_REMOTE_HPP_
