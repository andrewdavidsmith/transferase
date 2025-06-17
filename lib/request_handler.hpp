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

#ifndef LIB_REQUEST_HANDLER_HPP_
#define LIB_REQUEST_HANDLER_HPP_

#include "genome_index_set.hpp"
#include "methylome_set.hpp"

#include <cstdint>  // for std::uint32_t
#include <string>

namespace transferase {

struct request;
struct response_header;
struct query_container;
template <typename level_element_type> struct level_container;

/// @brief A handler that is shared by all connections; holds
/// methylomes and genome indexes in memory.
struct request_handler {
  request_handler(const std::string &methylome_dir,
                  const std::string &index_file_dir,
                  const std::uint32_t max_live_methylomes) :
    methylome_dir{methylome_dir}, index_file_dir{index_file_dir},
    methylomes(methylome_dir, max_live_methylomes), indexes(index_file_dir) {}

  // prevent copy; move disallowed because of std::mutex members in methylomes
  // and indexes
  // clang-format off
  request_handler(const request_handler &) = delete;
  request_handler &operator=(const request_handler &) = delete;
  request_handler(request_handler &&) noexcept = delete;
  request_handler &operator=(request_handler &&) noexcept = delete;
  // clang-format on

  /// @brief Construct the response header given the request, possibly
  /// while additional query information is still being received. This
  /// allows a response on certain errors allowing the client to
  /// cancel a large query that it may still be sending.
  auto
  handle_request(const request &req, response_header &resp_hdr) -> void;

  /// Handle a request to get levels for query intervals.
  template <typename lvl_elem_t>
  auto
  intervals_get_levels(const request &req, const query_container &query,
                       response_header &resp_hdr,
                       level_container<lvl_elem_t> &resp_data) -> void;

  /// Handle a request to get levels for query bins.
  template <typename lvl_elem_t>
  auto
  bins_get_levels(const request &req, response_header &resp_hdr,
                  level_container<lvl_elem_t> &resp_data) -> void;

  /// Handle a request to get levels for query windows.
  template <typename lvl_elem_t>
  auto
  windows_get_levels(const request &req, response_header &resp_hdr,
                     level_container<lvl_elem_t> &resp_data) -> void;

  /// @brief Directory on the local filesystem with methylomes
  std::string methylome_dir;
  /// @brief Directory on the local filesystem with genome indexes
  std::string index_file_dir;
  methylome_set methylomes;
  genome_index_set indexes;
};

}  // namespace transferase

#endif  // LIB_REQUEST_HANDLER_HPP_
