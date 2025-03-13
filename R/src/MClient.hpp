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

#include "genomic_interval.hpp"
#include "methylome_client_remote.hpp"

#include <Rcpp.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class MClient {
public:
  MClient();
  explicit MClient(const std::string &config_dir);

  auto
  tostring() const -> std::string;

  auto
  query_bins(const std::vector<std::string> &methylomes,
             const std::size_t bin_size) const -> Rcpp::NumericMatrix;

  auto
  query_intervals(const std::vector<std::string> &methylomes,
                  Rcpp::NumericMatrix intervals) const -> Rcpp::NumericMatrix;

private:
  std::unique_ptr<transferase::methylome_client_remote> client{nullptr};
};
