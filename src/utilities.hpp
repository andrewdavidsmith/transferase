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

#ifndef SRC_UTILITIES_HPP_
#define SRC_UTILITIES_HPP_

/*
  Functions declared here are used by multiple source files
 */

#include <boost/describe.hpp>
#include <boost/mp11/algorithm.hpp>

#include <algorithm>  // IWYU pragma: keep
#include <cctype>     // for isgraph, std::isgraph
#include <chrono>
#include <cstdint>   // for std::uint32_t
#include <iterator>  // for std::size
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::true_type
#include <utility>
#include <variant>  // for std::tuple
#include <vector>

namespace transferase {

[[nodiscard]] auto
get_server_config_dir_default(std::error_code &ec) -> std::string;

}  // namespace transferase

[[nodiscard]] auto
clean_path(const std::string &s, std::error_code &ec) -> std::string;

[[nodiscard]] inline auto
split_comma(const auto &s) {
  auto nonempty = [](const auto &x) { return !x.empty(); };
  return s | std::views::split(',') | std::views::transform([](const auto r) {
           return std::string(std::cbegin(r), std::cend(r));
         }) |
         std::views::filter(nonempty) |
         std::ranges::to<std::vector<std::string>>();
}

[[nodiscard]] inline auto
duration(const auto start, const auto stop) {
  const auto d = stop - start;
  return std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
}

[[nodiscard]] inline auto
rstrip(const char *const x) -> const std::string_view {
  const std::string s{x};
  const auto start = s.find_first_not_of("\n\r");
  return std::string_view(x + start, x + std::size(s));
}

[[nodiscard]] inline auto
rlstrip(const std::string &s) noexcept -> std::string {
  constexpr auto is_graph = [](const unsigned char c) {
    return std::isgraph(c);
  };
  const auto start = std::ranges::find_if(s, is_graph);
  auto stop = std::begin(std::ranges::find_last_if(s, is_graph));
  if (stop != std::cend(s))
    ++stop;
  return std::string(start, stop);
}

[[nodiscard]] auto
split_equals(const std::string &line, std::error_code &error) noexcept
  -> std::tuple<std::string, std::string>;

[[nodiscard]] auto
parse_config_file(const std::string &filename, std::error_code &error) noexcept
  -> std::vector<std::tuple<std::string, std::string>>;

template <class T>
auto
assign_member_impl(T &t, const std::string_view value) -> std::error_code {
  std::istringstream is(std::string(std::cbegin(value), std::cend(value)));
  if (is >> t)
    return {};
  return std::make_error_code(std::errc::invalid_argument);
}

template <class Scope>
auto
assign_member(Scope &scope, const std::string_view name,
              const std::string_view value) -> std::error_code {
  namespace dsc = boost::describe;
  // ADS: below, the (mod_public | mod_inherited) makes visible all
  // public members and those from a base class. Added when using
  // client_config_python inherting from client_config.
  using Md = dsc::describe_members<Scope, dsc::mod_public | dsc::mod_inherited>;
  std::error_code error{};
  boost::mp11::mp_for_each<Md>([&](const auto &D) {
    if (!error && name == D.name)
      error = assign_member_impl(scope.*D.pointer, value);
  });
  return error;
}

[[nodiscard]] inline auto
assign_members(const auto &key_val, auto &obj) -> std::error_code {
  std::error_code error;
  for (const auto &[key, value] : key_val) {
    std::string name(key);
    std::ranges::replace(name, '-', '_');
    error = assign_member(obj, name, value);
    if (error)
      return {};
  }
  return error;
}

[[nodiscard]]
auto
check_output_file(const std::string &filename) -> std::error_code;

enum class output_file_error_code : std::uint8_t {
  ok = 0,
  is_a_directory = 1,
  failed_to_open = 2,
};

template <>
struct std::is_error_code_enum<output_file_error_code> : public std::true_type {
};

struct output_file_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "output_file_error_code";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "is a directory"s;
    case 2: return "failed to open"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(output_file_error_code e) -> std::error_code {
  static auto category = output_file_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_UTILITIES_HPP_
