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

#include "methylome_name_list.hpp"

#include "methylome.hpp"

#include "nlohmann/json.hpp"

#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <system_error>
#include <vector>

namespace transferase {

[[nodiscard]] auto
methylome_name_list::get_genome(const std::vector<std::string> &methylome_names,
                                std::error_code &error) const noexcept
  -> std::string {
  [[maybe_unused]] const bool all_valid =
    methylome::are_valid_names(methylome_names, error);
  if (error)
    return {};
  std::string genome;
  for (const auto &name : methylome_names) {
    const auto genome_itr = methylome_to_genome.find(name);
    if (genome_itr == std::cend(methylome_to_genome)) {
      // ERROR not found
      error = methylome_name_list_error_code::methylome_name_not_found;
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
methylome_name_list::read(const std::string &json_filename,
                          std::error_code &error) noexcept
  -> methylome_name_list {
  constexpr auto read_error = methylome_name_list_error_code::read_error;
  constexpr auto parse_error = methylome_name_list_error_code::parse_error;

  std::ifstream in(json_filename);
  if (!in) {
    error = read_error;
    return {};
  }
  const nlohmann::json payload = nlohmann::json::parse(in, nullptr, false);
  if (payload.is_discarded()) {
    error = parse_error;
    return {};
  }

  std::map<std::string, std::vector<std::string>> data;
  try {
    data = payload;
  }
  catch (const nlohmann::json::exception &) {
    error = parse_error;
    return {};
  }

  methylome_name_list m;
  m.genome_to_methylomes = data;

  for (const auto &genome : m.genome_to_methylomes) {
    const auto &genome_name = genome.first;
    for (const auto &methylome_name : genome.second)
      m.methylome_to_genome.emplace(methylome_name, genome_name);
  }

  return m;
}

[[nodiscard]] auto
methylome_name_list::tostring() const -> std::string {
  static constexpr auto n_indent = 4;
  nlohmann::json data = *this;
  return data.dump(n_indent);
}

}  // namespace transferase
