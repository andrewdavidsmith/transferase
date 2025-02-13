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

#ifndef LIB_INCLUDE_CONFIG_FILE_UTILS_HPP_
#define LIB_INCLUDE_CONFIG_FILE_UTILS_HPP_

#include <boost/describe.hpp>
#include <boost/mp11/algorithm.hpp>  // for mp_for_each

#include <algorithm>  // for std::ranges::replace
#include <cerrno>
#include <format>
#include <fstream>
#include <print>
#include <ranges>  // IWYU pragma: keep
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::remove_cvref_t
#include <variant>      // for std::tuple
#include <vector>

namespace transferase {

[[nodiscard]] auto
parse_config_file_as_key_val(const std::string &filename,
                             std::error_code &error) noexcept
  -> std::vector<std::tuple<std::string, std::string>>;

[[nodiscard]] inline auto
format_as_config(const auto &t) -> std::string {
  using T = std::remove_cvref_t<decltype(t)>;
  using members =
    boost::describe::describe_members<T, boost::describe::mod_any_access |
                                           boost::describe::mod_inherited>;
  std::string r;
  boost::mp11::mp_for_each<members>([&](const auto &member) {
    std::string name(member.name);
    std::ranges::replace(name, '_', '-');
    const auto value = std::format("{}", t.*member.pointer);
    if (!value.empty())
      r += std::format("{} = {}\n", name, value);
    else
      r += std::format("# {} =\n", name);
  });
  return r;
}

inline auto
assign_member_impl(auto &t, const std::string &value) {
  std::istringstream(value) >> t;
}

inline auto
assign_member(auto &t, const std::string_view name, const auto &value) {
  namespace bd = boost::describe;
  using T = std::remove_cvref_t<decltype(t)>;
  using Md = bd::describe_members<T, bd::mod_any_access | bd::mod_inherited>;
  boost::mp11::mp_for_each<Md>([&](const auto &D) {
    if (name == D.name)
      assign_member_impl(t.*D.pointer, value);
  });
}

inline auto
parse_config_file(auto &t, const std::string &filename,
                  std::error_code &error) -> void {
  const auto key_vals = parse_config_file_as_key_val(filename, error);
  if (error)
    return;
  // attempt to assign each value to some variable of the class
  for (const auto &[key, val] : key_vals) {
    std::string name(key);
    std::ranges::replace(name, '-', '_');
    assign_member(t, name, val);
  }
}

[[nodiscard]] inline auto
// cppcheck-suppress unusedFunction
write_config_file(const auto &obj,
                  const std::string &config_file) -> std::error_code {
  std::ofstream out(config_file);
  if (!out)
    return std::make_error_code(std::errc(errno));
  std::print(out, "{}", format_as_config(obj));
  if (!out)
    return std::make_error_code(std::errc(errno));
  return std::error_code{};
}

}  // namespace transferase

#endif  // LIB_INCLUDE_CONFIG_FILE_UTILS_HPP_
