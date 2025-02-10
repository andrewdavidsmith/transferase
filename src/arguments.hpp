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

#include <boost/exception/diagnostic_information.hpp>
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
  name() const noexcept -> const char * override {return "argument_error_code";}
  auto message(int code) const -> std::string override {
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
  bool skip_parsing_config_file{false};

  // clang-format off
  auto self() -> T & {return static_cast<T &>(*this);}
  auto self() const -> const T & {return static_cast<const T &>(*this);}
  // clang-format on

  auto
  // cppcheck-suppress unusedFunction
  log_options() const {
    self().log_options_impl();
  }

  [[nodiscard]] static auto
  // cppcheck-suppress unusedFunction
  get_default_config_file() -> std::string {
    return T::get_default_config_file_impl();
  }

  [[nodiscard]] auto
  set_opts() -> boost::program_options::options_description {
    return self().set_opts_impl();
  }

  [[nodiscard]] auto
  set_hidden() -> boost::program_options::options_description {
    // This function should cover all options that go unused but might
    // appear in the config file because there is no easy way to treat
    // the config file differently from the cli when it comes to
    // unregistered options.
    return self().set_hidden_impl();
  }

  [[nodiscard]] auto
  parse(const int argc, const char *const argv[], const std::string &usage,
        const std::string &about_msg,
        const std::string &description_msg) -> std::error_code {
    namespace po = boost::program_options;
    const auto opts = set_opts();
    const auto hidden = set_hidden();
    po::options_description all;
    all.add(opts).add(hidden);
    try {
      po::variables_map var_map;
      po::store(po::parse_command_line(argc, argv, all), var_map);
      // check if help has been seen
      if (var_map.count("help") || argc == 1) {
        std::println("{}\n{}", about_msg, usage);
        opts.print(std::cout);
        std::println("\n{}", description_msg);
        return argument_error_code::help_requested;
      }

      // attempt to use the config file if it was explicitly
      // specified, or if it was defaulted and exists.
      if (!skip_parsing_config_file) {
        const bool config_file_defaulted = var_map["config-file"].defaulted();
        config_file = var_map["config-file"].as<std::string>();
        const bool use_config_file =
          !config_file.empty() &&
          (!config_file_defaulted || std::filesystem::exists(config_file));
        if (use_config_file)
          po::store(po::parse_config_file(config_file.data(), all), var_map);
      }
      po::notify(var_map);
    }
    catch (po::error &e) {
      std::println("{}", e.what());
      std::println("{}\n{}", about_msg, usage);
      opts.print(std::cout);
      std::println("\n{}", description_msg);
      return argument_error_code::failure;
    }
    return argument_error_code::ok;
  }
};

#endif  // SRC_ARGUMENTS_HPP_
