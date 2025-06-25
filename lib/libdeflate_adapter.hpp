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

#ifndef LIB_LIBDEFLATE_ADAPTER_HPP_
#define LIB_LIBDEFLATE_ADAPTER_HPP_

#include <libdeflate.h>

#include <cassert>
#include <cstdint>   // for std::uint8_t
#include <iterator>  // for std::size
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable
#include <vector>

enum class libdeflate_error_code : std::uint8_t {
  ok = 0,
  compression_failed = 1,
  decompression_failed = 2,
  bad_data = 3,
  short_output = 4,
  unexpected_return_code = 5,
};

template <>
struct std::is_error_code_enum<libdeflate_error_code> : public std::true_type {
};

struct libdeflate_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "libdeflate_error_code";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "LIBDEFLATE_SUCCESS"s;
    case 1: return "LIBDEFLATE_COMPRESSION_FAILED"s;
    case 2: return "LIBDEFLATE_DECOMPRESSION_FAILED"s;
    case 3: return "LIBDEFLATE_BAD_DATA"s;
    case 4: return "LIBDEFLATE_SHORT_OUTPUT"s;
    case 5: return "unexpected return code from libdeflate"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline std::error_code
make_error_code(libdeflate_error_code e) {
  static auto category = libdeflate_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

namespace transferase {

template <typename T>
[[nodiscard]] auto
libdeflate_compress(const T &in,
                    std::vector<std::uint8_t> &out) -> std::error_code {
  static constexpr int compression_level = 1;  // level 1 is fastest
  auto compressor = libdeflate_alloc_compressor(compression_level);
  if (!compressor)
    return libdeflate_error_code::compression_failed;

  const auto input_data = reinterpret_cast<const void *>(in.data());
  const std::size_t input_size = std::size(in) * sizeof(typename T::value_type);

  // estimate max output size
  const std::size_t max_compressed_size =
    libdeflate_deflate_compress_bound(compressor, input_size);

  out.resize(max_compressed_size);
  const std::size_t actual_compressed_size = libdeflate_deflate_compress(
    compressor, input_data, input_size, out.data(), max_compressed_size);

  // release the compressor
  libdeflate_free_compressor(compressor);

  if (actual_compressed_size == 0)
    return libdeflate_error_code::compression_failed;

  out.resize(actual_compressed_size);
  return libdeflate_error_code::ok;
}

template <typename T>
[[nodiscard]] static inline auto
libdeflate_decompress(std::vector<std::uint8_t> &in,
                      T &out) -> std::error_code {
  auto decompressor = libdeflate_alloc_decompressor();
  if (!decompressor)
    return libdeflate_error_code::decompression_failed;

  auto out_buf = reinterpret_cast<void *>(out.data());
  const std::size_t out_size = std::size(out) * sizeof(typename T::value_type);

  // clang-format off
  const libdeflate_result result = libdeflate_deflate_decompress(
    decompressor,
    in.data(),
    std::size(in),
    out_buf,
    out_size,
    nullptr  // don't care about actual output size
  );
  // clang-format on

  libdeflate_free_decompressor(decompressor);

  switch (result) {
  case LIBDEFLATE_SUCCESS:
    return libdeflate_error_code::ok;
  case LIBDEFLATE_BAD_DATA:
    return libdeflate_error_code::bad_data;
  case LIBDEFLATE_SHORT_OUTPUT:
    return libdeflate_error_code::short_output;
  case LIBDEFLATE_INSUFFICIENT_SPACE:
    return libdeflate_error_code::decompression_failed;
  default:
    assert(false && "Unknown libdeflate result");
    return libdeflate_error_code::decompression_failed;
  }
}

}  // namespace transferase

#endif  // LIB_LIBDEFLATE_ADAPTER_HPP_
