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

#include "client_connection.hpp"
#include "genome_index.hpp"
#include "level_container.hpp"
#include "methylome.hpp"
#include "request.hpp"

#include "nlohmann/json.hpp"

#include <string>
#include <vector>

namespace transferase {

class methylome_interface {
public:
  std::string directory;
  std::string hostname;
  std::string port_number;
  bool local_mode{false};

  [[nodiscard]] auto
  tostring() const noexcept -> std::string {
    static constexpr auto n_indent = 4;
    nlohmann::json data = *this;
    return data.dump(n_indent);
  }

  // intervals: takes a query
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const request &req, const query_container &query,
             std::error_code &ec) const noexcept
    -> level_container<lvl_elem_t> {
    return local_mode ? get_levels_local_impl<lvl_elem_t>(req, query, ec)
                      : get_levels_remote_impl<lvl_elem_t>(req, query, ec);
  }

  // bins: takes an index
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const request &req, const genome_index &index, std::error_code &ec)
    const noexcept -> level_container<lvl_elem_t> {
    return local_mode ? get_levels_local_impl<lvl_elem_t>(req, index, ec)
                      : get_levels_remote_impl<lvl_elem_t>(req, ec);
  }

private:
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_remote_impl(const request &req, const query_container &query,
                         std::error_code &ec) const noexcept
    -> level_container<lvl_elem_t> {
    intervals_client<lvl_elem_t> cl(hostname, port_number, req, query);
    ec = cl.run();
    if (ec)
      return {};
    return cl.take_levels();
  }

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_local_impl(const request &req, const query_container &query,
                        std::error_code &ec) const noexcept
    -> level_container<lvl_elem_t> {
    level_container<lvl_elem_t> results(req.n_intervals(), req.n_methylomes());
    auto col_itr = std::begin(results);
    for (const auto &methylome_name : req.methylome_names) {
      const auto meth = methylome::read(directory, methylome_name, ec);
      if (ec)
        return {};
      // This should take the results as out-param and have results fully
      // pre-allocated
      meth.get_levels<lvl_elem_t>(query, col_itr);
    }
    return results;
  }

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_remote_impl(const request &req, std::error_code &ec) const noexcept
    -> level_container<lvl_elem_t> {
    bins_client<lvl_elem_t> cl(hostname, port_number, req);
    ec = cl.run();
    if (ec)
      return {};
    return cl.take_levels();
  }

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_local_impl(const request &req, const genome_index &index,
                        std::error_code &ec) const noexcept
    -> level_container<lvl_elem_t> {
    level_container<lvl_elem_t> results(index.get_n_bins(req.bin_size()),
                                        req.n_methylomes());
    auto col_itr = std::begin(results);
    for (const auto &methylome_name : req.methylome_names) {
      const auto meth = methylome::read(directory, methylome_name, ec);
      if (ec)
        return {};
      meth.get_levels<lvl_elem_t>(req.bin_size(), index, col_itr);
    }
    return results;
  }

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(methylome_interface, directory, hostname,
                                 port_number)
};

}  // namespace transferase

#endif  // LIB_METHYLOME_INTERFACE_HPP_
