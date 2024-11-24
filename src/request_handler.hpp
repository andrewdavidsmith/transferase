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

#ifndef SRC_REQUEST_HANDLER_HPP_
#define SRC_REQUEST_HANDLER_HPP_

#include "cpg_index_set.hpp"
#include "methylome_set.hpp"
#include "request.hpp"
#include "response.hpp"

#include <string>

// handles all incoming requests
struct request_handler {
  request_handler(const request_handler &) = delete;
  request_handler &operator=(const request_handler &) = delete;

  explicit request_handler(const std::string &methylome_dir,
                           const std::string &index_file_dir,
                           const std::uint32_t max_live_methylomes) :
    methylome_dir{methylome_dir}, index_file_dir{index_file_dir},
    ms(max_live_methylomes, methylome_dir), indexes(index_file_dir) {}

  auto handle_header(const request_header &req_hdr,
                     response_header &resp_hdr) -> void;

  auto handle_get_counts(const request_header &req_hdr, const request &req,
                         response_header &resp_hdr,
                         response_payload &resp) -> void;

  auto handle_get_bins(const request_header &req_hdr, const bins_request &req,
                       response_header &resp_hdr,
                       response_payload &resp) -> void;

  std::string methylome_dir;   // dir of available methylomes
  std::string index_file_dir;  // dir of cpg index files
  methylome_set ms;
  cpg_index_set indexes;
};

#endif  // SRC_REQUEST_HANDLER_HPP_
