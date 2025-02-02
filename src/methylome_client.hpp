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

#ifndef SRC_METHYLOME_CLIENT_HPP_
#define SRC_METHYLOME_CLIENT_HPP_

#include "client.hpp"
#include "query_container.hpp"  // for transferase::size
#include "request.hpp"
#include "request_type_code.hpp"  // for transferase::request_type_code

#include <boost/describe.hpp>
#include <boost/json.hpp>

#include <cstdint>
#include <format>
#include <sstream>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable
#include <vector>

namespace transferase {
template <typename level_element_type> struct level_container;
}

namespace transferase {

class methylome_client {
public:
  std::string hostname;
  std::string port_number;
  std::uint64_t index_hash{};

  [[nodiscard]] static auto
  get_default(std::error_code &error) noexcept -> methylome_client;

#ifndef TRANSFERASE_NOEXCEPT
  [[nodiscard]] static auto
  get_default() -> methylome_client {
    std::error_code error;
    auto result = get_default(error);
    if (error)
      throw std::system_error(error);
    return result;
  }
#endif

  [[nodiscard]] auto
  tostring() const noexcept -> std::string {
    std::ostringstream o;
    if (!(o << boost::json::value_from(*this)))
      o.clear();
    return o.str();
  }

  // intervals: takes a query
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::vector<std::string> &methylome_names,
             const query_container &query, std::error_code &error)
    const noexcept -> std::vector<level_container<lvl_elem_t>> {
    static constexpr auto REQ_TYPE = request_type_code::intervals;
    const auto req =
      request{REQ_TYPE, index_hash, size(query), methylome_names};
    return get_levels_impl<lvl_elem_t>(req, query, error);
  }

#ifndef TRANSFERASE_NOEXCEPT
  // intervals: takes a query (throws)
  template <typename lvl_elem_t>
  auto
  get_levels(const std::vector<std::string> &methylome_names,
             const query_container &query) const
    -> std::vector<level_container<lvl_elem_t>> {
    std::error_code error;
    auto result = get_levels<lvl_elem_t>(methylome_names, query, error);
    if (error)
      throw std::system_error(error);
    return result;
  }
#endif

  // bins: takes an index
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::vector<std::string> &methylome_names,
             const std::uint32_t bin_size, std::error_code &error)
    const noexcept -> std::vector<level_container<lvl_elem_t>> {
    static constexpr auto REQ_TYPE = request_type_code::bins;
    const auto req = request{REQ_TYPE, index_hash, bin_size, methylome_names};
    return get_levels_impl<lvl_elem_t>(req, error);
  }

#ifndef TRANSFERASE_NOEXCEPT
  // bins: takes an index (throws)
  template <typename lvl_elem_t>
  auto
  get_levels(const std::vector<std::string> &methylome_names,
             const std::uint32_t bin_size) const
    -> std::vector<level_container<lvl_elem_t>> {
    std::error_code error;
    auto result = get_levels<lvl_elem_t>(methylome_names, bin_size, error);
    if (error)
      throw std::system_error(error);
    return result;
  }
#endif

private:
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const request &req, const query_container &query,
                  std::error_code &error) const noexcept
    -> std::vector<level_container<lvl_elem_t>> {
    intervals_client<lvl_elem_t> cl(hostname, port_number, req, query);
    error = cl.run();
    if (error)
      return {};
    return cl.take_levels(error);
  }

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const request &req, std::error_code &error) const noexcept
    -> std::vector<level_container<lvl_elem_t>> {
    bins_client<lvl_elem_t> cl(hostname, port_number, req);
    error = cl.run();
    if (error)
      return {};
    return cl.take_levels(error);
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(methylome_client, (),
(
 hostname,
 port_number,
 index_hash
))
// clang-format on

}  // namespace transferase

/// @brief Enum for error codes related to methylome_client
enum class methylome_client_error_code : std::uint8_t {
  ok = 0,
  error_reading_config_file = 1,
  required_config_values_not_found = 2,
};

template <>
struct std::is_error_code_enum<methylome_client_error_code>
  : public std::true_type {};

struct methylome_client_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "methylome_client"; }
  auto message(int code) const noexcept -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error reading default config file"s;
    case 2: return "required config values not found"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(methylome_client_error_code e) noexcept -> std::error_code {
  static auto category = methylome_client_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_METHYLOME_CLIENT_HPP_
