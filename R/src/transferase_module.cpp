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

#include "MClient.hpp"

#include "level_element.hpp"
#include "logger.hpp"
#include "query_container.hpp"
#include "system_config.hpp"  // for get_system_config_filename

#include <Rcpp.h>

#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

[[nodiscard]] static inline auto
find_dir(const std::vector<std::string> &paths,
         const std::string &filename) -> std::string {
  for (const auto &p : paths) {
    // Some of the paths given by the environment might not exist
    const auto path_elem_exists = std::filesystem::exists(p);
    if (!path_elem_exists)
      continue;
    const auto dir_itr = std::filesystem::recursive_directory_iterator(p);
    for (const auto &dir_entry : dir_itr) {
      const auto &curr_dir = dir_entry.path();
      if (std::filesystem::is_directory(curr_dir)) {
        const auto candidate = curr_dir / filename;
        if (std::filesystem::exists(candidate))
          return curr_dir.string();
      }
    }
  }
  throw std::runtime_error(
    std::format("Failed to locate system config file: {}", filename));
}

[[nodiscard]] static inline auto
get_package_paths() -> std::vector<std::string> {
  static constexpr auto raw_lib_paths = ".libPaths";
  const Rcpp::Function lib_paths_fun(raw_lib_paths);
  const auto lib_paths = lib_paths_fun();
  std::vector<std::string> results;
  for (auto i = 0u; i < LENGTH(lib_paths); ++i) {
    const auto curr_path = STRING_PTR(lib_paths)[i];
    results.emplace_back(CHAR(curr_path), LENGTH(curr_path));
  }
  return results;
}

[[nodiscard]] static inline auto
find_R_sys_config_dir() -> std::string {
  const auto sys_conf_file = transferase::get_system_config_filename();
  const auto package_paths = get_package_paths();
  return find_dir(package_paths, sys_conf_file);
}

static auto
configure_transferase(const std::vector<std::string> &genomes,
                      const std::string &config_dir) -> void {
  try {
    const auto download_policy = transferase::download_policy_t::update;
    const auto sys_config_dir = find_R_sys_config_dir();
    transferase::client_config cfg(config_dir, sys_config_dir);
    cfg.install(genomes, download_policy, sys_config_dir);
  }
  catch (const std::exception &e) {
    Rcpp::Rcout << std::format("{}\n", e.what());
  }
}

static auto
set_log_level(const std::string &log_level_name) -> void {
  const auto itr = transferase::str_to_level.find(log_level_name);
  if (itr == std::cend(transferase::str_to_level)) {
    Rcpp::Rcout << std::format("Invalid log level: {}\n", log_level_name);
    return;
  }
  transferase::logger::set_level(itr->second);
}

RCPP_EXPOSED_CLASS_NODECL(transferase::query_container)
RCPP_EXPOSED_CLASS_NODECL(transferase::level_element_t)
RCPP_EXPOSED_CLASS_NODECL(transferase::level_element_covered_t)

RCPP_MODULE(transferase) {
  using namespace Rcpp;

  namespace xfr = transferase;

  // ADS: this is not to set the log level, but instead to prevent a crash due
  // to the log output not existing.
  [[maybe_unused]] auto &lgr =
    xfr::logger::instance(std::make_shared<std::ostream>(Rcpp::Rcout.rdbuf()),
                          "transferase", xfr::log_level_t::error);

  Rcpp::class_<MClient<xfr::level_element_t>>("MClient")
    .constructor()
    .constructor<std::string>()
    .method("tostring", &MClient<xfr::level_element_t>::tostring,
            "documentation")
    .method("format_query", &MClient<xfr::level_element_t>::format_query,
            "documentation")
    .method("query_bins", &MClient<xfr::level_element_t>::query_bins,
            "documentation")
    .method("query_intervals", &MClient<xfr::level_element_t>::query_intervals,
            "documentation")
    .method("query_preprocessed",
            &MClient<xfr::level_element_t>::query_preprocessed,
            "documentation");

  Rcpp::class_<MClient<xfr::level_element_covered_t>>("MClientCovered")
    .constructor()
    .constructor<std::string>()
    .method("tostring", &MClient<xfr::level_element_covered_t>::tostring,
            "documentation")
    .method("format_query",
            &MClient<xfr::level_element_covered_t>::format_query,
            "documentation")
    .method("query_bins", &MClient<xfr::level_element_covered_t>::query_bins,
            "documentation")
    .method("query_intervals",
            &MClient<xfr::level_element_covered_t>::query_intervals,
            "documentation")
    .method("query_preprocessed",
            &MClient<xfr::level_element_covered_t>::query_preprocessed,
            "documentation");

  Rcpp::class_<xfr::query_container>("QueryContainer").constructor();

  Rcpp::function("transferase_config", &configure_transferase, "documentation");
  Rcpp::function("transferase_set_log_level", &set_log_level, "documentation");
}
