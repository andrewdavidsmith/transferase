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

#ifndef SRC_METHYLOME_SERVER_HPP_
#define SRC_METHYLOME_SERVER_HPP_

#include "client.hpp"
#include "genome_index.hpp"
#include "methylome.hpp"
#include "request.hpp"

#include <boost/json.hpp>

#include <string>
#include <vector>

namespace transferase {

class methylome_server {
public:
  std::string hostname;
  std::string port_number;
  std::uint64_t index_hash{};

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
BOOST_DESCRIBE_STRUCT(methylome_server, (),
(
 hostname,
 port_number,
 index_hash
))
// clang-format on

}  // namespace transferase

#endif  // SRC_METHYLOME_SERVER_HPP_
