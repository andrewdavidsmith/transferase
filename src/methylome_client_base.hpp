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

#ifndef SRC_METHYLOME_CLIENT_BASE_HPP_
#define SRC_METHYLOME_CLIENT_BASE_HPP_

#include "client_config.hpp"
#include "genome_index.hpp"  // for genome_index::list
#include "genome_index_set.hpp"
#include "query_container.hpp"  // for transferase::size
#include "request.hpp"
#include "request_type_code.hpp"  // for transferase::request_type_code
#include "transferase_metadata.hpp"

#include <boost/describe.hpp>
#include <boost/json.hpp>
#include <boost/mp11.hpp>

#include <cstdint>
#include <format>
#include <memory>  // for std::shared_ptr
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable
#include <variant>      // for std::tuple
#include <vector>

// forward declarations
namespace transferase {
struct level_element_covered_t;
template <typename level_element_type> struct level_container;
struct genome_index_set;
}  // namespace transferase

/// @brief Enum for error codes related to methylome_client_base
enum class methylome_client_base_error_code : std::uint8_t {
  ok = 0,
  error_reading_config_file = 1,
  required_config_values_not_found = 2,
  index_dir_not_found = 3,
  failed_to_read_index_dir = 4,
  transferase_metadata_not_found = 5,
};

template <>
struct std::is_error_code_enum<methylome_client_base_error_code>
  : public std::true_type {};

struct methylome_client_base_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "methylome_client_base"; }
  auto message(int code) const noexcept -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error reading default config file"s;
    case 2: return "required config values not found"s;
    case 3: return "index dir not found"s;
    case 4: return "failed to read index dir"s;
    case 5: return "transferase metadata not found"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(methylome_client_base_error_code e) noexcept
  -> std::error_code {
  static auto category = methylome_client_base_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

namespace transferase {

template <typename Derived> class methylome_client_base {
public:
  client_config config;
  std::shared_ptr<genome_index_set> indexes;

  // clang-format off
  auto self() -> Derived & {return static_cast<Derived &>(*this);}
  auto self() const -> const Derived & {return static_cast<const Derived &>(*this);}
  // clang-format on

  [[nodiscard]] auto
  tostring() const noexcept -> std::string {
    return self().tostring_derived();
  }

#ifndef TRANSFERASE_NOEXCEPT
  [[nodiscard]] auto
  configured_genomes() const -> std::vector<std::string> {
    std::error_code error;
    auto obj = genome_index::list(config.index_dir, error);
    if (error)
      throw std::system_error(error);
    return obj;
  }
#endif

#ifndef TRANSFERASE_NOEXCEPT
  // API function
  [[nodiscard]] static auto
  get_client(std::string config_dir) -> Derived {
    std::error_code error;
    if (config_dir.empty()) {
      config_dir = client_config::get_default_config_dir(error);
      if (error) {
        const auto msg = "[Failed to get default config dir]";
        throw std::system_error(error, msg);
      }
    }
    auto client = get_client_impl(config_dir, error);
    if (error) {
      const auto msg =
        std::format("[Failed to get client (config dir: {})]", config_dir);
      throw std::system_error(error, msg);
    }
    return client;
  }
#endif

  // intervals: takes a query
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::vector<std::string> &methylome_names,
             const query_container &query, std::error_code &error)
    const noexcept -> std::vector<level_container<lvl_elem_t>> {
    return self().template get_levels_derived<lvl_elem_t>(methylome_names,
                                                          query, error);
  }

#ifndef TRANSFERASE_NOEXCEPT
  // intervals: takes a query
  template <typename lvl_elem_t>
  auto
  get_levels(const std::vector<std::string> &methylome_names,
             const query_container &query) const
    -> std::vector<level_container<lvl_elem_t>> {
    std::error_code error;
    auto result = get_levels<lvl_elem_t>(methylome_names, query, error);
    if (error)
      throw std::system_error(error);
    return result;
  }
#endif

  // bins: takes an index
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::vector<std::string> &methylome_names,
             const std::uint32_t bin_size, std::error_code &error)
    const noexcept -> std::vector<level_container<lvl_elem_t>> {
    return self().template get_levels_derived<lvl_elem_t>(methylome_names,
                                                          bin_size, error);
  }

#ifndef TRANSFERASE_NOEXCEPT
  // bins: takes an index (throws)
  template <typename lvl_elem_t>
  auto
  get_levels(const std::vector<std::string> &methylome_names,
             const std::uint32_t bin_size) const
    -> std::vector<level_container<lvl_elem_t>> {
    std::error_code error;
    auto result = get_levels<lvl_elem_t>(methylome_names, bin_size, error);
    if (error)
      throw std::system_error(error);
    return result;
  }
#endif

protected:
  auto
  tostring_derived() const = delete;

  auto
  validate_derived() const = delete;

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_derived(const std::vector<std::string> &methylome_names,
                     const query_container &query, std::error_code &error)
    const noexcept -> std::vector<level_container<lvl_elem_t>> = delete;

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_derived(const std::vector<std::string> &methylome_names,
                     const std::uint32_t bin_size, std::error_code &error)
    const noexcept -> std::vector<level_container<lvl_elem_t>>;

  [[nodiscard]] auto
  get_index_hash(const std::string &genome_name,
                 std::error_code &error) const noexcept -> std::uint64_t {
    const auto index = indexes->get_genome_index(genome_name, error);
    if (error)  // ADS: need to confirm error code here
      return 0;
    return index->get_hash();
  }

  [[nodiscard]] auto
  get_genome_and_index_hash(const std::vector<std::string> &methylome_names,
                            std::error_code &error) const noexcept
    -> std::tuple<std::string, std::uint64_t> {
    const auto genome_name = config.meta.get_genome(methylome_names, error);
    if (error)  // ADS: need to confirm error code here
      return {std::string{}, 0};
    return {genome_name, get_index_hash(genome_name, error)};
  }

private:
  // internal function
  [[nodiscard]] static auto
  get_client_impl(const std::string &config_dir,
                  std::error_code &error) -> Derived {
    Derived client;
    // The client_config::read function should read the transferase
    // metadata if possible
    client.config = client_config::read(config_dir, error);
    if (error)
      return {};

    // validate for derived class
    client.validate_derived(error);
    if (error)
      return {};

    // No error associated with bad index dir; that should be taken care of
    // elsewhere
    if (!client.config.index_dir.empty())
      client.indexes =
        std::make_shared<genome_index_set>(client.config.index_dir);

    return client;
  }

public:
  BOOST_DESCRIBE_CLASS(methylome_client_base, (), (config), (), ())
};

template <
  class T,
  class D1 = boost::describe::describe_members<
    T, boost::describe::mod_any_access | boost::describe::mod_inherited>,
  class D2 = boost::describe::describe_members<T, boost::describe::mod_private>,
  class En = std::enable_if_t<boost::mp11::mp_empty<D2>::value &&
                              !std::is_union<T>::value>>
auto
tag_invoke(const boost::json::value_from_tag &, boost::json::value &v,
           const T &t) -> void {
  auto &obj = v.emplace_object();
  boost::mp11::mp_for_each<D1>(
    [&](auto D) { obj[D.name] = boost::json::value_from(t.*D.pointer); });
}

}  // namespace transferase

#endif  // SRC_METHYLOME_CLIENT_BASE_HPP_
