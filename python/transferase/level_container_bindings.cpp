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

#include "level_container_bindings.hpp"

#include <level_container.hpp>
#include <level_element.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>  // IWYU pragma: keep

#include <cstddef>  // for std::size_t
#include <cstdint>  // for std::uint32_t
#include <stdexcept>
#include <tuple>
#include <variant>  // for std::tuple

namespace py = pybind11;

namespace transferase {

[[nodiscard]] inline auto
level_element_t_to_tuple(const level_element_t &self)
  -> std::tuple<std::uint32_t, std::uint32_t> {
  return std::make_tuple(self.n_meth, self.n_unmeth);
};

[[nodiscard]] inline auto
level_element_covered_t_to_tuple(const level_element_covered_t &self)
  -> std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> {
  return std::make_tuple(self.n_meth, self.n_unmeth, self.n_covered);
};

}  // namespace transferase

auto
level_container_bindings(
  py::class_<transferase::level_container<transferase::level_element_t>> &cls)
  -> void {
  using namespace pybind11::literals;  // NOLINT
  namespace tra = transferase;
  cls.def(py::init<>())
    .def(
      "__getitem__",
      [](const tra::level_container<tra::level_element_t> &self,
         const std::size_t pos) -> std::tuple<std::uint32_t, std::uint32_t> {
        if (pos >= tra::size(self))
          throw std::out_of_range("Index out of range");
        return tra::level_element_t_to_tuple(self[pos]);
      },
      "Access element at position 'pos' in this container by copying it into a "
      "tuple of the values [n_meth, n_unmeth]",
      "pos"_a)
    .def(
      "get_n_meth",
      [](const tra::level_container<tra::level_element_t> &self,
         const std::size_t pos) -> std::uint32_t {
        if (pos >= tra::size(self))
          throw std::out_of_range("Index out of range");
        return self[pos].n_meth;
      },
      "Access the 'n_meth' value via copy for the element at position 'pos' in "
      "this container",
      "pos"_a)
    .def(
      "get_n_unmeth",
      [](const tra::level_container<tra::level_element_t> &self,
         const std::size_t pos) -> std::uint32_t {
        if (pos >= tra::size(self))
          throw std::out_of_range("Index out of range");
        return self[pos].n_unmeth;
      },
      "Access the 'n_unmeth' value via copy for the element at position 'pos' "
      "in this container",
      "pos"_a)
    .def("__str__",
         [](const tra::level_container<tra::level_element_t> &self)
           -> std::string {
           return std::format("LevelContainer size={}", tra::size(self));
         })
    //
    ;
}

auto
level_container_covered_bindings(
  py::class_<transferase::level_container<transferase::level_element_covered_t>>
    &cls) -> void {
  using namespace pybind11::literals;  // NOLINT
  namespace tra = transferase;
  cls.def(py::init<>())
    .def(
      "__getitem__",
      [](const tra::level_container<tra::level_element_covered_t> &self,
         const std::size_t pos)
        -> std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> {
        if (pos >= tra::size(self))
          throw std::out_of_range("Index out of range");
        return tra::level_element_covered_t_to_tuple(self[pos]);
      },
      "Access element at position 'pos' in this container by copying it into a "
      "tuple of the values [n_meth, n_unmeth]",
      "pos"_a)
    .def(
      "get_n_meth",
      [](const tra::level_container<tra::level_element_covered_t> &self,
         const std::size_t pos) -> std::uint32_t {
        if (pos >= tra::size(self))
          throw std::out_of_range("Index out of range");
        return self[pos].n_meth;
      },
      "Access the 'n_meth' value via copy for the element at position 'pos' in "
      "this container",
      "pos"_a)
    .def(
      "get_n_unmeth",
      [](const tra::level_container<tra::level_element_covered_t> &self,
         const std::size_t pos) -> std::uint32_t {
        if (pos >= tra::size(self))
          throw std::out_of_range("Index out of range");
        return self[pos].n_unmeth;
      },
      "Access the 'n_unmeth' value via copy for the element at position 'pos' "
      "in this container",
      "pos"_a)
    .def(
      "get_n_covered",
      [](const tra::level_container<tra::level_element_covered_t> &self,
         const std::size_t pos) -> std::uint32_t {
        if (pos >= tra::size(self))
          throw std::out_of_range("Index out of range");
        return self[pos].n_covered;
      },
      "Access the 'n_covered' value via copy for the element at position 'pos' "
      "in this container ",
      "pos"_a)
    .def("__str__",
         [](const tra::level_container<tra::level_element_covered_t> &self)
           -> std::string {
           return std::format("LevelContainerCovered size={}", tra::size(self));
         })
    //
    ;
}
