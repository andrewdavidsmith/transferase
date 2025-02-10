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

#ifndef PYTHON_TRANSFERASE_KWARGS_INIT_HELPER_HPP_
#define PYTHON_TRANSFERASE_KWARGS_INIT_HELPER_HPP_

#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <nanobind/nanobind.h>

#include <format>
#include <string>
#include <string_view>

namespace nb = nanobind;

template <class T>
auto
assign_member_impl(T &t, const std::string_view /*name*/,
                   const nb::handle &value) -> void {
  t = value.cast<T>();  // may throw cast_error
}

// for const members
template <class T>
auto
assign_member_impl(const T & /*t*/, const std::string_view name,
                   const nb::handle & /*value*/) -> void {
  throw std::invalid_argument(std::format("{}: cannot be modified", name));
}

template <class Scope>
auto
assign_member(Scope &scope, const std::string_view name,
              const nb::handle &value) -> void {
  using Md =
    boost::describe::describe_members<Scope, boost::describe::mod_public>;
  bool found = false;
  boost::mp11::mp_for_each<Md>([&](const auto &D) {
    if (!found && name == D.name) {
      assign_member_impl(scope.*D.pointer, name, value);
      found = true;
    }
  });
  if (!found)
    throw std::invalid_argument(std::format("variable not found: {}", name));
}

template <typename T>
[[nodiscard]] auto
kwargs_init_helper(const nb::kwargs &kwargs) -> T {
  T t{};
  for (const auto &k : kwargs) {
    const auto name = k.first.cast<std::string>();  // may throw cast_error
    try {
      assign_member(t, name, k.second);
    }
    catch (nb::cast_error &e) {
      throw nanobind::type_error(std::format("incorrect value for {}", name));
    }
  }
  return t;
}

#endif  // PYTHON_TRANSFERASE_KWARGS_INIT_HELPER_HPP_
