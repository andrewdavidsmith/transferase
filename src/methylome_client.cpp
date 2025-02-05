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

#include "methylome_client.hpp"
#include "client_config.hpp"
#include "genome_index.hpp"
#include "genome_index_set.hpp"
#include "methylome_genome_map.hpp"
#include "utilities.hpp"

#include <boost/mp11/algorithm.hpp>  // for boost::mp11::mp_for_each

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <iterator>  // for std::cbegin, std::cend
#include <memory>    // for std::make_shared
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace transferase {

[[nodiscard]] auto
methylome_client::available_genomes([[maybe_unused]] std::error_code &error)
  const noexcept -> std::vector<std::string> {
  return lookup.genome_to_methylomes | std::views::elements<0> |
         std::ranges::to<std::vector<std::string>>();
}

[[nodiscard]] auto
methylome_client::configured_genomes(std::error_code &error) const noexcept
  -> std::vector<std::string> {
  return genome_index::list(index_dir, error);
}

[[nodiscard]] auto
methylome_client::get_index_hash(const std::string &genome_name,
                                 std::error_code &error) const noexcept
  -> std::uint64_t {
  const auto index = indexes->get_genome_index(genome_name, error);
  if (error)  // ADS: need to confirm error code here
    return 0;
  return index->get_hash();
}

[[nodiscard]] auto
methylome_client::get_genome_and_index_hash(
  const std::vector<std::string> &methylome_names, std::error_code &error)
  const noexcept -> std::tuple<std::string, std::uint64_t> {
  const auto genome_name = lookup.get_genome(methylome_names, error);
  if (error)  // ADS: need to confirm error code here
    return {std::string{}, 0};
  return {genome_name, get_index_hash(genome_name, error)};
}

[[nodiscard]] auto
methylome_client::initialize(std::error_code &error) noexcept
  -> methylome_client {
  auto client = methylome_client::read(error);
  if (error)
    return {};
  return client;
}

auto
methylome_client::reset_to_default_configuration_system_config(
  const std::string &config_dir, const std::string &system_config,
  std::error_code &error) noexcept -> void {
  client_config config;
  config.set_defaults_system_config(config_dir, system_config, error);
  if (error)
    return;
  // ADS: (doto) check that this function below should not have out param
  error = config.make_directories(config_dir);
  if (error)
    return;
  config.write(config_dir, error);
}

auto
methylome_client::reset_to_default_configuration_system_config(
  const std::string &system_config, std::error_code &error) noexcept -> void {
  const std::string config_dir = client_config::get_config_dir_default(error);
  if (error)
    return;

  client_config config;
  config.set_defaults_system_config(config_dir, system_config, error);
  if (error)
    return;
  // ADS: (doto) check that this function below should not have out param
  error = config.make_directories(config_dir);
  if (error)
    return;
  config.write(config_dir, error);
}

auto
methylome_client::reset_to_default_configuration(
  const std::string &config_dir, std::error_code &error) noexcept -> void {
  client_config config;
  config.set_defaults(config_dir, error);
  if (error)
    return;
  error = config.make_directories(config_dir);
  if (error)
    return;
  config.write(config_dir, error);
}

auto
methylome_client::reset_to_default_configuration(
  std::error_code &error) noexcept -> void {
  const std::string config_dir = client_config::get_config_dir_default(error);
  if (error)
    return;

  client_config config;
  config.set_defaults(config_dir, error);
  if (error)
    return;
  error = config.make_directories(config_dir);
  if (error)
    return;
  config.write(config_dir, error);
}

template <class T>
auto
assign_member_impl(T &t, const std::string_view value) -> std::error_code {
  std::string tmp(std::cbegin(value), std::cend(value));
  std::istringstream is(tmp);
  if (!(is >> t))
    return std::make_error_code(std::errc::invalid_argument);
  return {};
}

template <class Scope>
auto
assign_member(Scope &scope, const std::string_view name,
              const std::string_view value) -> std::error_code {
  using Md =
    boost::describe::describe_members<Scope, boost::describe::mod_public>;
  std::error_code error{};
  boost::mp11::mp_for_each<Md>([&](const auto &D) {
    if (!error && name == D.name) {
      error = assign_member_impl(scope.*D.pointer, value);
    }
  });
  return error;
}

[[nodiscard]] inline auto
assign_members(const std::vector<std::tuple<std::string, std::string>> &key_val,
               auto &cfg) -> std::error_code {
  std::error_code error;
  for (const auto &[key, value] : key_val) {
    std::string name(key);
    std::ranges::replace(name, '-', '_');
    error = assign_member(cfg, name, value);
    if (error)
      return {};
  }
  return error;
}

[[nodiscard]] auto
methylome_client::read(const std::string &config_dir,
                       std::error_code &error) noexcept -> methylome_client {
  const auto config_file = client_config::get_config_file(config_dir, error);
  if (error)
    return {};

  std::ifstream in(config_file);
  if (!in) {
    error = std::make_error_code(std::errc(errno));
    return {};
  }

  std::vector<std::tuple<std::string, std::string>> key_val;
  std::string line;
  while (getline(in, line)) {
    line = rlstrip(line);
    // ignore empty lines and those beginning with '#'
    if (!line.empty() && line[0] != '#') {
      const auto [k, v] = split_equals(line, error);
      if (error)
        return {};
      key_val.emplace_back(k, v);
    }
  }

  methylome_client c;
  error = assign_members(key_val, c);
  if (error)
    return {};

  if (c.hostname.empty() || c.port.empty() || c.index_dir.empty() ||
      c.metadata_file.empty()) {
    error = methylome_client_error_code::required_config_values_not_found;
    return {};
  }

  c.indexes = std::make_shared<genome_index_set>(c.index_dir);

  c.lookup = methylome_genome_map::read(c.metadata_file, error);
  if (error) {
    // keep the error from reading the methylome lookup
    return {};
  }

  return c;
}

[[nodiscard]] auto
methylome_client::read(std::error_code &error) noexcept -> methylome_client {
  const auto config_dir = client_config::get_config_dir_default(error);
  if (error)
    // error from get_config_dir_default
    return {};
  return read(config_dir, error);
}

/// Write the client configuration to the given directory.
auto
methylome_client::write(const std::string &config_dir,
                        std::error_code &error) const noexcept -> void {
  client_config config;
  config.hostname = hostname;
  config.port = port;
  config.index_dir = index_dir;
  config.metadata_file = metadata_file;
  // ADS: (doto) check that this function below should not have out param
  error = config.make_directories(config_dir);
  if (error)
    return;
  config.write(config_dir, error);
}

/// Write the client configuration to the default directory.
auto
methylome_client::write(std::error_code &error) const noexcept -> void {
  const auto config_dir = client_config::get_config_dir_default(error);
  if (error)
    return;
  write(config_dir, error);
}

}  // namespace transferase
