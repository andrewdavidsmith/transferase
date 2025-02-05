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

#ifndef SRC_METHYLOME_CLIENT_HPP_
#define SRC_METHYLOME_CLIENT_HPP_

#include "client.hpp"
#include "genome_index_set.hpp"
#include "methylome_genome_map.hpp"
#include "query_container.hpp"  // for transferase::size
#include "request.hpp"
#include "request_type_code.hpp"  // for transferase::request_type_code

#include <boost/describe.hpp>
#include <boost/json.hpp>

#include <cstdint>
#include <format>
#include <sstream>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable
#include <vector>

namespace transferase {
template <typename level_element_type> struct level_container;
}

namespace transferase {

class methylome_client {
public:
  std::string hostname;
  std::string port;
  /// @brief Directory on the local filesystem with genome indexes
  std::string index_dir;
  /// @brief Directory on the local filesystem with metadata information
  std::string metadata_file;
  std::shared_ptr<genome_index_set> indexes;
  methylome_genome_map lookup;

  [[nodiscard]] auto
  available_genomes([[maybe_unused]] std::error_code &error) const noexcept
    -> std::vector<std::string>;

#ifndef TRANSFERASE_NOEXCEPT
  [[nodiscard]] auto
  available_genomes() const -> std::vector<std::string> {
    std::error_code error;
    auto obj = available_genomes(error);
    if (error)
      throw std::system_error(error);
    return obj;
  }
#endif

  [[nodiscard]] auto
  configured_genomes(std::error_code &error) const noexcept
    -> std::vector<std::string>;

#ifndef TRANSFERASE_NOEXCEPT
  [[nodiscard]] auto
  configured_genomes() const -> std::vector<std::string> {
    std::error_code error;
    auto obj = configured_genomes(error);
    if (error)
      throw std::system_error(error);
    return obj;
  }
#endif

  /// Read the client configuration.
  [[nodiscard]] static auto
  read(const std::string &config_dir,
       std::error_code &error) noexcept -> methylome_client;

#ifndef TRANSFERASE_NOEXCEPT
  /// Overload of the read function that throws system_error exceptions to serve
  /// in an API
  [[nodiscard]] static auto
  read(const std::string &config_dir) -> methylome_client {
    std::error_code error;
    const auto obj = read(config_dir, error);
    if (error)
      throw std::system_error(error);
    return obj;
  }
#endif

  /// Read the client configuration.
  [[nodiscard]] static auto
  read(std::error_code &error) noexcept -> methylome_client;

#ifndef TRANSFERASE_NOEXCEPT
  /// Overload of the read function that throws system_error exceptions to serve
  /// in an API
  [[nodiscard]] static auto
  read() -> methylome_client {
    std::error_code error;
    const auto obj = read(error);
    if (error)
      throw std::system_error(error);
    return obj;
  }
#endif

  /// Write the client configuration to the given directory.
  auto
  write(const std::string &config_dir,
        std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Write the client configuration to the given directory, throwing
  /// system_error exceptions to serve in an API
  auto
  write(const std::string &config_dir) const -> void {
    std::error_code error;
    write(config_dir, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  /// Write the client configuration to the default configuration
  /// directory.
  auto
  write(std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Write the client configuration to the given directory, throwing
  /// system_error exceptions to serve in an API
  auto
  write() const -> void {
    std::error_code error;
    write(error);
    if (error)
      throw std::system_error(error);
  }
#endif

  static auto
  reset_to_default_configuration_system_config(
    const std::string &config_dir, const std::string &system_config,
    std::error_code &error) noexcept -> void;

  static auto
  reset_to_default_configuration_system_config(
    const std::string &system_config, std::error_code &error) noexcept -> void;

  static auto
  reset_to_default_configuration(const std::string &config_dir,
                                 std::error_code &error) noexcept -> void;

  static auto
  reset_to_default_configuration(std::error_code &error) noexcept -> void;

  [[nodiscard]] static auto
  initialize(std::error_code &error) noexcept -> methylome_client;

#ifndef TRANSFERASE_NOEXCEPT
  [[nodiscard]] static auto
  initialize() -> methylome_client {
    std::error_code error;
    auto result = initialize(error);
    if (error)
      throw std::system_error(error);
    return result;
  }
#endif

  [[nodiscard]] auto
  tostring() const noexcept -> std::string {
    std::ostringstream o;
    if (!(o << boost::json::value_from(*this)))
      o.clear();
    return o.str();
  }

  // intervals: takes a query
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::vector<std::string> &methylome_names,
             const query_container &query, std::error_code &error)
    const noexcept -> std::vector<level_container<lvl_elem_t>> {
    static constexpr auto REQ_TYPE = request_type_code::intervals;
    const auto [_, index_hash] =
      get_genome_and_index_hash(methylome_names, error);
    const auto req =
      request{REQ_TYPE, index_hash, size(query), methylome_names};
    return get_levels_impl<lvl_elem_t>(req, query, error);
  }

#ifndef TRANSFERASE_NOEXCEPT
  // intervals: takes a query (throws)
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
    static constexpr auto REQ_TYPE = request_type_code::bins;
    const auto [_, index_hash] =
      get_genome_and_index_hash(methylome_names, error);
    const auto req = request{REQ_TYPE, index_hash, bin_size, methylome_names};
    return get_levels_impl<lvl_elem_t>(req, error);
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

private:
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const request &req, const query_container &query,
                  std::error_code &error) const noexcept
    -> std::vector<level_container<lvl_elem_t>> {
    intervals_client<lvl_elem_t> cl(hostname, port, req, query);
    error = cl.run();
    if (error)
      return {};
    return cl.take_levels(error);
  }

  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels_impl(const request &req, std::error_code &error) const noexcept
    -> std::vector<level_container<lvl_elem_t>> {
    bins_client<lvl_elem_t> cl(hostname, port, req);
    error = cl.run();
    if (error)
      return {};
    return cl.take_levels(error);
  }

  [[nodiscard]] auto
  get_index_hash(const std::string &genome,
                 std::error_code &error) const noexcept -> std::uint64_t;

  [[nodiscard]] auto
  get_genome_and_index_hash(const std::vector<std::string> &methylome_names,
                            std::error_code &error) const noexcept
    -> std::tuple<std::string, std::uint64_t>;
};

// clang-format off
BOOST_DESCRIBE_STRUCT(methylome_client, (),
(
 hostname,
 port,
 index_dir,
 metadata_file
))
// clang-format on

}  // namespace transferase

/// @brief Enum for error codes related to methylome_client
enum class methylome_client_error_code : std::uint8_t {
  ok = 0,
  error_reading_config_file = 1,
  required_config_values_not_found = 2,
};

template <>
struct std::is_error_code_enum<methylome_client_error_code>
  : public std::true_type {};

struct methylome_client_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "methylome_client"; }
  auto message(int code) const noexcept -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error reading default config file"s;
    case 2: return "required config values not found"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(methylome_client_error_code e) noexcept -> std::error_code {
  static auto category = methylome_client_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_METHYLOME_CLIENT_HPP_
