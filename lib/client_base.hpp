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

#ifndef LIB_CLIENT_BASE_HPP_
#define LIB_CLIENT_BASE_HPP_

#include "client_config.hpp"
#include "genome_index.hpp"  // for genome_index::list
#include "genome_index_set.hpp"
#include "genomic_interval.hpp"
#include "level_container_md.hpp"
#include "methylome_name_list.hpp"
#include "query_container.hpp"  // for transferase::size
#include "request.hpp"
#include "request_type_code.hpp"  // for transferase::request_type_code

#include "nlohmann/json.hpp"

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
struct genome_index_set;
}  // namespace transferase

/// @brief Enum for error codes related to client_base
enum class client_error_code : std::uint8_t {
  ok = 0,
  error_reading_config_file = 1,
  required_config_values_not_found = 2,
  index_dir_not_configured = 3,
  failed_to_read_index_dir = 4,
  methylome_dir_not_configured = 5,
  inconsistent_methylome_metadata = 6,
  hostname_not_configured = 7,
  port_not_configured = 8,
};

template <>
struct std::is_error_code_enum<client_error_code> : public std::true_type {};

struct client_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "client_base"; }
  auto message(int code) const noexcept -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error reading config file"s;
    case 2: return "required config values not found"s;
    case 3: return "index dir not configured"s;
    case 4: return "failed to read index dir"s;
    case 5: return "methylome dir not configured"s;
    case 6: return "inconsistent methylome metadata"s;
    case 7: return "hostname not configured"s;
    case 8: return "port not configured"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(client_error_code e) noexcept -> std::error_code {
  static auto category = client_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

namespace transferase {

template <typename Derived> class client_base {
public:
  client_config config;
  std::shared_ptr<genome_index_set> indexes;

  [[nodiscard]] auto
  tostring() const -> std::string {
    static constexpr auto n_indent = 4;
    nlohmann::json data = *this;
    return data.dump(n_indent);
  }

  [[nodiscard]] auto
  configured_genomes() const -> std::vector<std::string> {
    std::error_code error;
    auto obj = genome_index::list(config.get_index_dir(), error);
    if (error)
      throw std::system_error(error);
    return obj;
  }

protected:
  explicit client_base(std::string config_dir) {
    std::error_code error;
    if (config_dir.empty()) {
      config_dir = client_config::get_default_config_dir(error);
      if (error)
        throw std::system_error(error, "[Failed to get default config dir]");
    }
    // client_config should read the transferase metadata if possible
    config = client_config::read(config_dir);

    // Error for index dir should be taken care of in client_config
    if (!config.index_dir.empty())
      indexes = std::make_shared<genome_index_set>(config.get_index_dir());
  }

  client_base() = default;
  client_base(const std::string &config_dir, std::error_code &error) noexcept :
    config(config_dir, error) {}

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(client_base, config)
};

}  // namespace transferase

#endif  // LIB_CLIENT_BASE_HPP_
