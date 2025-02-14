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

#ifndef LIB_METHYLOME_INTERFACE_HPP_
#define LIB_METHYLOME_INTERFACE_HPP_

#include "client.hpp"
#include "genome_index.hpp"
#include "methylome.hpp"
#include "request.hpp"

#include <boost/json.hpp>

#include <string>
#include <vector>

namespace transferase {

class methylome_interface {
public:
  std::string directory;
  std::string hostname;
  std::string port_number;

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
  get_levels(const request &req, const query_container &query,
             std::error_code &ec) const noexcept
    -> std::variant<std::vector<level_container<level_element_t>>,
                    std::vector<level_container<level_element_covered_t>>> {
    if (!directory.empty())
      return get_levels_local_impl<lvl_elem_t>(req, query, ec);
    else
      return get_levels_remote_impl<lvl_elem_t>(req, query, ec);
  }

  // bins: takes an index
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const request &req, const genome_index &index,
             std::error_code &ec) const noexcept
    -> std::variant<std::vector<level_container<level_element_t>>,
                    std::vector<level_container<level_element_covered_t>>> {
    if (!directory.empty())
      return get_levels_local_impl<lvl_elem_t>(req, index, ec);
    else
      return get_levels_remote_impl<lvl_elem_t>(req, ec);
  }

private:
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_remote_impl(const request &req, const query_container &query,
                         std::error_code &ec) const noexcept
    -> std::vector<level_container<lvl_elem_t>> {
    intervals_client<lvl_elem_t> cl(hostname, port_number, req, query);
    ec = cl.run();
    if (ec)
      return {};
    return cl.take_levels(ec);
  }

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_local_impl(const request &req, const query_container &query,
                        std::error_code &ec) const noexcept
    -> std::vector<level_container<lvl_elem_t>> {
    std::vector<level_container<lvl_elem_t>> results;
    for (const auto &methylome_name : req.methylome_names) {
      const auto meth = methylome::read(directory, methylome_name, ec);
      if (ec)
        return {};
      results.emplace_back(meth.get_levels<lvl_elem_t>(query));
    }
    return results;
  }

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_remote_impl(const request &req, std::error_code &ec) const noexcept
    -> std::vector<level_container<lvl_elem_t>> {
    bins_client<lvl_elem_t> cl(hostname, port_number, req);
    ec = cl.run();
    if (ec)
      return {};
    return cl.take_levels(ec);
  }

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_local_impl(const request &req, const genome_index &index,
                        std::error_code &ec) const noexcept
    -> std::vector<level_container<lvl_elem_t>> {
    std::vector<level_container<lvl_elem_t>> results;
    for (const auto &methylome_name : req.methylome_names) {
      const auto meth = methylome::read(directory, methylome_name, ec);
      if (ec)
        return {};
      results.emplace_back(meth.get_levels<lvl_elem_t>(req.bin_size(), index));
    }
    return results;
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(methylome_interface, (),
(
 directory,
 hostname,
 port_number
))
// clang-format on

}  // namespace transferase

#endif  // LIB_METHYLOME_INTERFACE_HPP_
