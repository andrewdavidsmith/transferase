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

#ifndef SRC_METHYLOME_RESOURCE_HPP_
#define SRC_METHYLOME_RESOURCE_HPP_

#include "client.hpp"
#include "cpg_index.hpp"
#include "methylome.hpp"
#include "request.hpp"

#include <boost/json.hpp>

#include <string>

namespace xfrase {

template <typename lvl_elem_type>
[[nodiscard]] static inline auto
get_levels_remote_impl(auto const &resource, request const &req,
                       query_container const &query,
                       std::error_code &ec) -> level_container<lvl_elem_type> {
  intervals_client<lvl_elem_type> cl(resource.hostname, resource.port_number,
                                     req, query);
  ec = cl.run();
  if (ec)
    return {};
  return cl.take_levels();
}

template <typename lvl_elem_type>
[[nodiscard]] static inline auto
get_levels_local_impl(auto const &resource, request const &req,
                      query_container const &query,
                      std::error_code &ec) -> level_container<lvl_elem_type> {
  const auto meth = methylome::read(resource.directory, req.accession, ec);
  if (ec)
    return {};
  if constexpr (std::is_same_v<lvl_elem_type, level_element_covered_t>)
    return meth.get_levels_covered(query);
  else
    return meth.get_levels(query);
}

template <typename lvl_elem_type>
[[nodiscard]] static inline auto
get_levels_remote_impl(auto const &resource, request const &req,
                       std::error_code &ec) -> level_container<lvl_elem_type> {
  bins_client<lvl_elem_type> cl(resource.hostname, resource.port_number, req);
  ec = cl.run();
  if (ec)
    return {};
  return cl.take_levels();
}

template <typename lvl_elem_type>
[[nodiscard]] static inline auto
get_levels_local_impl(auto const &resource, const request &req,
                      const cpg_index &index,
                      std::error_code &ec) -> level_container<lvl_elem_type> {
  const auto meth = methylome::read(resource.directory, req.accession, ec);
  if (ec)
    return {};
  if constexpr (std::is_same_v<lvl_elem_type, level_element_covered_t>)
    return meth.get_levels_covered(req.bin_size(), index);
  else
    return meth.get_levels(req.bin_size(), index);
}

class methylome_resource {
public:
  std::string directory;
  std::string hostname;
  std::string port_number;

  [[nodiscard]] auto
  tostring() const -> std::string {
    std::ostringstream o;
    if (!(o << boost::json::value_from(*this)))
      o.clear();
    return o.str();
  }

  /// intervals: takes a query
  template <typename lvl_elem_type>
  [[nodiscard]] auto
  get_levels(const request &req, const query_container &query,
             std::error_code &ec) const
    -> std::variant<level_container<level_element_t>,
                    level_container<level_element_covered_t>> {
    if (!directory.empty())
      return get_levels_local_impl<lvl_elem_type>(*this, req, query, ec);
    else
      return get_levels_remote_impl<lvl_elem_type>(*this, req, query, ec);
  }

  /// bins: takes an index
  template <typename lvl_elem_type>
  [[nodiscard]] auto
  get_levels(const request &req, const cpg_index &index, std::error_code &ec)
    const -> std::variant<level_container<level_element_t>,
                          level_container<level_element_covered_t>> {
    if (!directory.empty())
      return get_levels_local_impl<lvl_elem_type>(*this, req, index, ec);
    else
      return get_levels_remote_impl<lvl_elem_type>(*this, req, ec);
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(methylome_resource, (),
(
 directory,
 hostname,
 port_number
 ))
// clang-format on

class local_methylome_resource {
public:
  std::string directory;
  std::uint64_t index_hash;

  [[nodiscard]] auto
  tostring() const -> std::string {
    std::ostringstream o;
    if (!(o << boost::json::value_from(*this)))
      o.clear();
    return o.str();
  }

  /// intervals: takes a query
  [[nodiscard]] auto
  get_levels(const std::string &methylome_name, const query_container &query,
             std::error_code &ec) const -> level_container<level_element_t> {
    static constexpr auto REQUEST_TYPE = xfrase::request_type_code::intervals;
    const auto req = xfrase::request{methylome_name, REQUEST_TYPE, index_hash,
                                     xfrase::size(query)};
    return get_levels_local_impl<level_element_t>(*this, req, query, ec);
  }

  /// intervals: takes a query
  [[nodiscard]] auto
  get_levels_covered(const std::string &methylome_name,
                     const query_container &query, std::error_code &ec) const
    -> level_container<level_element_covered_t> {
    static constexpr auto REQUEST_TYPE =
      xfrase::request_type_code::intervals_covered;
    const auto req = xfrase::request{methylome_name, REQUEST_TYPE, index_hash,
                                     xfrase::size(query)};
    return get_levels_local_impl<level_element_covered_t>(*this, req, query,
                                                          ec);
  }

  /// bins: takes an index
  [[nodiscard]] auto
  get_levels(const std::string &methylome_name, const std::uint32_t bin_size,
             const cpg_index &index,
             std::error_code &ec) const -> level_container<level_element_t> {
    static constexpr auto REQUEST_TYPE = xfrase::request_type_code::intervals;
    const auto req =
      xfrase::request{methylome_name, REQUEST_TYPE, index_hash, bin_size};
    return get_levels_local_impl<level_element_t>(*this, req, index, ec);
  }

  /// bins: takes an index
  [[nodiscard]] auto
  get_levels_covered(const std::string &methylome_name,
                     const std::uint32_t bin_size, const cpg_index &index,
                     std::error_code &ec) const
    -> level_container<level_element_covered_t> {
    static constexpr auto REQUEST_TYPE =
      xfrase::request_type_code::intervals_covered;
    const auto req =
      xfrase::request{methylome_name, REQUEST_TYPE, index_hash, bin_size};
    return get_levels_local_impl<level_element_covered_t>(*this, req, index,
                                                          ec);
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(local_methylome_resource, (),
(
 directory,
 index_hash
 ))
// clang-format on

class remote_methylome_resource {
public:
  std::string hostname;
  std::string port_number;
  std::uint64_t index_hash;

  [[nodiscard]] auto
  tostring() const -> std::string {
    std::ostringstream o;
    if (!(o << boost::json::value_from(*this)))
      o.clear();
    return o.str();
  }

  /// intervals: takes a query
  [[nodiscard]] auto
  get_levels(const std::string &methylome_name, const query_container &query,
             std::error_code &ec) const -> level_container<level_element_t> {
    static constexpr auto REQUEST_TYPE = xfrase::request_type_code::intervals;
    const auto req = xfrase::request{methylome_name, REQUEST_TYPE, index_hash,
                                     xfrase::size(query)};
    return get_levels_remote_impl<level_element_t>(*this, req, query, ec);
  }

  /// intervals: takes a query
  [[nodiscard]] auto
  get_levels_covered(const std::string &methylome_name,
                     const query_container &query, std::error_code &ec) const
    -> level_container<level_element_covered_t> {
    static constexpr auto REQUEST_TYPE =
      xfrase::request_type_code::intervals_covered;
    const auto req = xfrase::request{methylome_name, REQUEST_TYPE, index_hash,
                                     xfrase::size(query)};
    return get_levels_remote_impl<level_element_covered_t>(*this, req, query,
                                                           ec);
  }

  /// bins: takes an index
  [[nodiscard]] auto
  get_levels(const std::string &methylome_name, const std::uint32_t bin_size,
             std::error_code &ec) const -> level_container<level_element_t> {
    static constexpr auto REQUEST_TYPE = xfrase::request_type_code::bins;
    const auto req =
      xfrase::request{methylome_name, REQUEST_TYPE, index_hash, bin_size};
    return get_levels_remote_impl<level_element_t>(*this, req, ec);
  }

  /// bins: takes an index
  [[nodiscard]] auto
  get_levels_covered(const std::string &methylome_name,
                     const std::uint32_t bin_size, std::error_code &ec) const
    -> level_container<level_element_covered_t> {
    static constexpr auto REQUEST_TYPE =
      xfrase::request_type_code::bins_covered;
    const auto req =
      xfrase::request{methylome_name, REQUEST_TYPE, index_hash, bin_size};
    return get_levels_remote_impl<level_element_covered_t>(*this, req, ec);
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(remote_methylome_resource, (),
(
 hostname,
 port_number,
 index_hash
 ))
// clang-format on

}  // namespace xfrase

#endif  // SRC_METHYLOME_RESOURCE_HPP_
