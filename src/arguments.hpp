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
#include <iostream>
#include <print>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable

enum class argument_error : std::uint8_t {
  ok = 0,
  help_requested = 1,
  failure = 2,
};

template <>
struct std::is_error_code_enum<argument_error> : public std::true_type {};

struct argument_error_category : std::error_category {
  const char *
  name() const noexcept override {
    return "argument_error";
  }
  std::string
  message(int code) const override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0:
      return "ok"s;
    case 1:
      return "help requested"s;
    case 2:
      return "failure parsing options"s;
    }
    std::unreachable();
  }
};

inline std::error_code
make_error_code(argument_error e) {
  static auto category = argument_error_category{};
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

  [[nodiscard]] auto
  parse(int argc, char *argv[], const std::string &usage,
        const std::string &about_msg,
        const std::string &description_msg) -> std::error_code {
    boost::program_options::options_description cli_only_opts =
      set_cli_only_opts();
    boost::program_options::options_description common_opts = set_common_opts();
    boost::program_options::options_description help_opts("Options");
    help_opts.add(cli_only_opts).add(common_opts);
    // first check if config file or help are specified
    try {
      boost::program_options::variables_map vm_cli_only;
      boost::program_options::store(
        boost::program_options::command_line_parser(argc, argv)
          .options(cli_only_opts)
          .allow_unregistered()
          .run(),
        vm_cli_only);
      if (vm_cli_only.count("help") || argc == 1) {
        // help output is for all options
        std::println("{}\n{}", about_msg, usage);
        help_opts.print(std::cout);
        std::println("\n{}", description_msg);
        return argument_error::help_requested;
      }
      boost::program_options::notify(vm_cli_only);
      boost::program_options::variables_map vm_common;
      const auto cli_parsed =
        boost::program_options::command_line_parser(argc, argv)
          .options(common_opts)
          .allow_unregistered()
          .run();
      boost::program_options::store(cli_parsed, vm_common);

      if (!config_file.empty()) {
        const auto cfg_parsed = boost::program_options::parse_config_file(
          config_file.data(), common_opts, true);
        boost::program_options::store(cfg_parsed, vm_common);
      }
      boost::program_options::notify(vm_common);
    }
    catch (boost::program_options::error &e) {
      std::println("{}", e.what());
      std::println("{}\n{}", about_msg, usage);
      help_opts.print(std::cout);
      std::println("\n{}", description_msg);
      return argument_error::failure;
    }
    return argument_error::ok;
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
