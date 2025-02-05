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

#include "methylome_genome_map.hpp"

#include <boost/json.hpp>

#include <cstdint>  // for uint32_t, uint64_t
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <ranges>
#include <string>
#include <system_error>
#include <vector>

namespace transferase {

[[nodiscard]] auto
methylome_genome_map::get_genome(
  const std::vector<std::string> &methylome_names,
  std::error_code &error) const noexcept -> std::string {
  std::string genome;
  for (const auto &name : methylome_names) {
    const auto genome_itr = methylome_to_genome.find(name);
    if (genome_itr == std::cend(methylome_to_genome)) {
      // ERROR not found
      error = std::make_error_code(std::errc::invalid_argument);
      return {};
    }
    if (!genome.empty() && genome != genome_itr->second) {
      // ERROR inconsistent genomes
      error = std::make_error_code(std::errc::invalid_argument);
      return {};
    }
    if (genome.empty())
      genome = genome_itr->second;
  }
  return genome;
}

[[nodiscard]] auto
methylome_genome_map::read(const std::string &json_filename,
                           std::error_code &error) noexcept
  -> methylome_genome_map {
  std::ifstream in(json_filename);
  if (!in) {
    error = methylome_genome_map_error_code::error_reading_metadata_json_file;
    return {};
  }

  const auto filesize = static_cast<std::streamsize>(
    std::filesystem::file_size(json_filename, error));
  if (error) {
    error = methylome_genome_map_error_code::error_reading_metadata_json_file;
    return {};
  }

  std::string payload(filesize, '\0');
  if (!in.read(payload.data(), filesize)) {
    error = methylome_genome_map_error_code::error_reading_metadata_json_file;
    return {};
  }

  std::map<std::string, std::map<std::string, std::string>> data;
  boost::json::parse_into(data, payload, error);
  if (error) {
    error = methylome_genome_map_error_code::error_parsing_metadata_json_file;
    return {};
  }

  methylome_genome_map l;
  std::ranges::for_each(data, [&](const auto &d) {
    l.genome_to_methylomes.emplace(
      d.first, d.second | std::views::elements<0> |
                 std::ranges::to<std::vector<std::string>>());
  });

  for (const auto &genome : l.genome_to_methylomes) {
    const auto &genome_name = genome.first;
    for (const auto &methylome_name : genome.second)
      l.methylome_to_genome.emplace(methylome_name, genome_name);
  }

  return l;
}

[[nodiscard]] auto
methylome_genome_map::string() const noexcept -> std::string {
  std::ostringstream o;
  if (!(o << boost::json::value_from(*this)))
    o.clear();
  return o.str();
}

}  // namespace transferase
