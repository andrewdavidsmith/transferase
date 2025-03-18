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

// ADS: This file is identical to the one in /lib except that it sets the
// "Stream" to "Rcout" for the R API.

#ifndef R_SRC_DOWNLOAD_PROGRESS_HPP_
#define R_SRC_DOWNLOAD_PROGRESS_HPP_

#include "indicators/indicators.hpp"

#include "asio.hpp"

#include <Rcpp.hpp>

#include <cstdint>
#include <cstdlib>
#include <format>
#include <string>
#include <system_error>

namespace transferase {

struct download_progress {
  static constexpr auto bar_width = 50u;
  indicators::ProgressBar bar;
  std::string label;
  double total_size{};
  std::uint32_t prev_percent{101};  // Set this to 0 to show progress
  download_progress() = default;
  explicit download_progress(const std::string &label) :
    bar{
      // clang-format off
      indicators::option::BarWidth{bar_width},
      indicators::option::Start{"["},
      indicators::option::Fill{"="},
      indicators::option::Lead{"="},
      indicators::option::Remainder{"-"},
      indicators::option::End{"]"},
      indicators::option::PostfixText{},
      indicators::option::Stream{Rcpp::Rcout},
      // clang-format on
    } {
    bar.set_option(
      indicators::option::PostfixText{std::format("Downloading: {}", label)});
  }

  /// Set the size of the file being downloaded; need a setter because we
  /// usually don't know the size when instantiating the progess bar
  auto
  set_total_size(const std::size_t &sz) -> void {
    total_size = sz;
    prev_percent = 0;
  }

  auto
  update(std::uint64_t bytes_downloaded) -> void {
    const std::uint32_t percent = (100.0 * bytes_downloaded) / total_size;
    if (percent > prev_percent) {
      bar.set_progress(percent);
      prev_percent = percent;
    }
  }
};

}  // namespace transferase

#endif  // R_SRC_DOWNLOAD_PROGRESS_HPP_
