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

#ifndef SRC_ARGUMENTS_HPP_
#define SRC_ARGUMENTS_HPP_

#include <boost/program_options.hpp>

#include <cstdint>  // for std::uint8_t
#include <filesystem>
#include <iostream>
#include <print>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable

enum class argument_error_code : std::uint8_t {
  ok = 0,
  help_requested = 1,
  failure = 2,
};

template <>
struct std::is_error_code_enum<argument_error_code> : public std::true_type {};

struct argument_error_code_category : std::error_category {
  // clang-format off
  auto
  name() const noexcept -> const char * override  {
    return "argument_error_code";
  }
  auto
  message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "help requested"s;
    case 2: return "failure parsing options"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(argument_error_code e) -> std::error_code {
  static auto category = argument_error_code_category{};
  return std::error_code(std::to_underlying(e), category);
}

template <typename T> struct argset_base {
  std::string config_file{};

  T &
  self() {
    return static_cast<T &>(*this);
  }
  const T &
  self() const {
    return static_cast<const T &>(*this);
  }

  [[nodiscard]] static auto
  get_default_config_file() -> std::string {
    return T::get_default_config_file_impl();
  }

  [[nodiscard]] static auto
  get_default_config_dir() -> std::string {
    return T::get_default_config_dir_impl();
  }

  [[nodiscard]] auto
  parse(int argc, char const *const argv[], const std::string &usage,
        const std::string &about_msg,
        const std::string &description_msg) -> std::error_code {
    namespace po = boost::program_options;
    auto cli_only_opts = set_cli_only_opts();
    auto common_opts = set_common_opts();
    po::options_description help_opts("Options");
    help_opts.add(cli_only_opts).add(common_opts);
    // first check if config file or help are specified
    try {
      // Command-line only options
      po::variables_map vm_cli_only;
      po::store(po::command_line_parser(argc, argv)
                  .options(cli_only_opts)
                  .allow_unregistered()
                  .run(),
                vm_cli_only);
      // check if help has been seen
      if (vm_cli_only.count("help") || argc == 1) {
        // help output is for all options
        std::println("{}\n{}", about_msg, usage);
        cli_only_opts.print(std::cout);
        std::println();
        common_opts.print(std::cout);
        std::println("\n{}", description_msg);
        return argument_error_code::help_requested;
      }
      // do the parsing -- this might throw
      po::notify(vm_cli_only);

      // Common options
      po::variables_map vm_common;
      const auto cli_parsed =
        po::command_line_parser(argc, argv).options(common_opts).run();
      po::store(cli_parsed, vm_common);

      // attempt to use the config file if it was explicitly specified
      // or if it exists
      const bool config_file_defaulted = vm_cli_only["config-file"].defaulted();
      const bool use_config_file =
        !config_file.empty() &&
        (!config_file_defaulted || std::filesystem::exists(config_file));
      if (use_config_file) {
        const auto cfg_parsed =
          po::parse_config_file(config_file.data(), common_opts, true);
        po::store(cfg_parsed, vm_common);
      }
      po::notify(vm_common);
    }
    catch (po::error &e) {
      std::println("{}", e.what());
      std::println("{}\n{}", about_msg, usage);
      cli_only_opts.print(std::cout);
      std::println();
      common_opts.print(std::cout);
      std::println("\n{}", description_msg);
      return argument_error_code::failure;
    }
    return argument_error_code::ok;
  }

  auto
  log_options() const {
    self().log_options_impl();
  }

  [[nodiscard]] auto
  set_cli_only_opts() -> boost::program_options::options_description {
    return self().set_cli_only_opts_impl();
  }

  [[nodiscard]] auto
  set_common_opts() -> boost::program_options::options_description {
    return self().set_common_opts_impl();
  }
};

#endif  // SRC_ARGUMENTS_HPP_
