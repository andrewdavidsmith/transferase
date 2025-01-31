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

#include "command_select.hpp"

#ifdef HAVE_NCURSES

static constexpr auto about = R"(
select methylomes based on metadata related to biological samples
)";

static constexpr auto description = R"(
This command interacts with MethBase2 metadata files for experiments,
allowing methylomes to be selected based on information about the
associated biological samples. This command uses a text-based user
interface with list navigation. A genome must be specified because the
selection can only be done for one genome at a time. The selected
methylomes are output to a text file with one methylome accession per
line. The purpose of this file is to serve as input for transferase
queries.
)";

static constexpr auto examples = R"(
Examples:

xfr select -o output_file.txt -g hg38
)";

#include "client_config.hpp"
#include "utilities.hpp"

#include <boost/container/detail/std_fwd.hpp>  // for std::pair
#include <boost/json.hpp>
#include <boost/program_options.hpp>

#include <ncurses.h>

#include <algorithm>
#include <cctype>  // for std::isprint
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>  // for std::getchar
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <print>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>  // for std::pair
#include <vector>

[[nodiscard]] auto
load_data(const std::string &json_filename, std::error_code &error)
  -> std::vector<std::pair<std::string, std::string>> {
  std::ifstream in(json_filename);
  if (!in) {
    error = std::make_error_code(std::errc(errno));
    return {};
  }

  const auto filesize = static_cast<std::streamsize>(
    std::filesystem::file_size(json_filename, error));
  if (error)
    return {};

  std::string payload(filesize, '\0');
  if (!in.read(payload.data(), filesize)) {
    error = std::make_error_code(std::errc(errno));
    return {};
  }

  std::map<std::string, std::string> data;
  boost::json::parse_into(data, payload, error);
  if (error) {
    error = std::make_error_code(std::errc(errno));
    return {};
  }

  return data |
         std::ranges::to<std::vector<std::pair<std::string, std::string>>>();
}

static auto
mvprintw(const int x, const int y, const std::string &s) {
  const auto ret = mvprintw(x, y, "%s", s.data());  // NOLINT
  if (ret != OK)
    throw std::runtime_error(
      std::format("Error updating display (writing: {})", s));
}

[[nodiscard]] static inline auto
get_to_show(const auto &filtered, const auto disp_start, const auto disp_end) {
  const auto i = std::cbegin(filtered);
  return std::ranges::subrange(i + disp_start, i + disp_end);
}

[[nodiscard]]
static inline auto
format_current_entry(const auto &entry, const auto horiz_pos, const auto max_x,
                     const auto margin) {
  return std::format("{}: {}", entry.first,
                     entry.second.substr(horiz_pos, max_x - margin));
}

static inline auto
do_select(const auto &filtered, const auto cursor_pos, auto &selected_keys) {
  const auto curr_fltr = std::cbegin(filtered) + cursor_pos;
  const auto itr = selected_keys.find(curr_fltr->first);
  if (itr == std::cend(selected_keys))
    selected_keys.insert(curr_fltr->first);
  else
    selected_keys.erase(itr);
}

static inline auto
do_add(const auto &filtered, const auto cursor_pos, auto &selected_keys) {
  const auto curr_fltr = std::cbegin(filtered) + cursor_pos;
  const auto itr = selected_keys.find(curr_fltr->first);
  if (itr == std::cend(selected_keys))
    selected_keys.insert(curr_fltr->first);
}

static inline auto
do_remove(const auto &filtered, const auto cursor_pos, auto &selected_keys) {
  const auto curr_fltr = std::cbegin(filtered) + cursor_pos;
  const auto itr = selected_keys.find(curr_fltr->first);
  if (itr != std::cend(selected_keys))
    selected_keys.erase(curr_fltr->first);
}

static inline auto
show_selected_keys(const auto &selected_keys) {
  using std::string_literals::operator""s;
  clear();
  mvprintw(0, 0, "Selected keys: "s);
  if (selected_keys.empty())
    mvprintw(1, 0, "Empty selection."s);
  else {
    const auto joined = selected_keys | std::views::join_with(',') |
                        std::ranges::to<std::string>();
    mvprintw(1, 0, joined);
  }
  refresh();
  getch();  // any key will go back to list for selection
}

static inline auto
get_query(std::string &query, std::regex &query_re) {
  static constexpr auto escape_key_code = 27;
  static constexpr auto enter_key_code = 10;
  query.clear();
  clear();
  mvprintw(1, 0, std::format("Search Query: {}", query));
  refresh();
  while (true) {
    const auto query_ch = getch();
    if (query_ch == escape_key_code)
      break;  // ESC to cancel
    else if (query_ch == enter_key_code)
      break;  // Enter to submit
    else if (std::isprint(query_ch) || query_ch == ' ')
      query += static_cast<char>(query_ch);
    else if (query_ch == KEY_BACKSPACE || query_ch == KEY_DC)
      if (!query.empty())
        query.pop_back();
    clear();
    mvprintw(1, 0, std::format("Search Query: {}", query));
    refresh();
  }
  query_re = std::regex(query, std::regex_constants::icase);
}

[[nodiscard]] static inline auto
confirm_quit(const auto &selected_keys) -> bool {
  using std::string_literals::operator""s;
  const auto message =
    selected_keys.empty()
      ? std::format("Quit? [y/n]")
      : std::format("Quit and save selection ({} items)? [y/n]",
                    std::size(selected_keys));
  int confirmation{};
  while (std::tolower(confirmation) != 'y' &&
         std::tolower(confirmation) != 'n') {
    erase();
    mvprintw(0, 0, message);
    refresh();
    confirmation = std::getchar();
  }
  erase();
  mvprintw(0, 0, message + std::string(" {}", confirmation));
  refresh();
  return (confirmation == 'y' || confirmation == 'Y');
}

auto
main_loop(const std::vector<std::pair<std::string, std::string>> &data)
  -> std::vector<std::string> {
  static constexpr auto extra_margin_space = 3;
  static constexpr auto escape_key_code = 27;
  static constexpr auto escape_delay{25};
  static constexpr auto legend =
    "q=Quit, Move=Arrows, a=Toggle multi-add, r=Toggle multi-remove, "
    "Space=Add/Remove, v=View selected, c=Clear selected, Line={}/{}";

  // margin must be max key width plus room
  const auto key_sizes = std::views::transform(
    data, [](const auto &s) -> std::int32_t { return std::size(s.first); });
  const std::int32_t margin = std::ranges::max(key_sizes) + extra_margin_space;

  // Initialize ncurses
  initscr();
  set_escdelay(escape_delay);
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);

  // Set up color pairs
  start_color();
  init_pair(1, COLOR_WHITE, COLOR_BLACK);   // Normal text
  init_pair(2, COLOR_YELLOW, COLOR_BLACK);  // Highlighted current item
  init_pair(3, COLOR_GREEN, COLOR_BLACK);   // Selected items
  init_pair(4, COLOR_BLUE, COLOR_BLACK);    // Multiple selection mode active

  std::unordered_set<std::string> selected_keys;
  std::string query;
  std::regex query_re;
  bool multi_add = false;
  bool multi_remove = false;
  std::int32_t horiz_pos = 0;
  std::int32_t cursor_pos = 0;
  std::int32_t legend_height = 1;  // lines to keep at top of display
  std::vector<std::pair<std::string, std::string>> filtered;

  while (true) {
    // Filter data based on the query
    const auto query_found = [&](const auto &x) -> bool {
      // ADS: need to take care of upper/lower case here
      return std::regex_search(x.second, query_re);
    };
    filtered.clear();
    if (!query.empty())
      std::ranges::copy_if(data, std::back_inserter(filtered), query_found);

    if (filtered.empty()) {
      query.clear();
      filtered = data;
    }

    // NOLINTBEGIN (cppcoreguidelines-narrowing-conversions)
    const std::int32_t n_filtered = std::size(filtered);
    // NOLINTEND (cppcoreguidelines-narrowing-conversions)

    // Include the query in the legened if appropriate
    const std::string current_legend =
      query.empty() ? std::format(legend, cursor_pos + 1, n_filtered)
                    : std::format(legend, cursor_pos + 1, n_filtered) +
                        std::format(" [{}]", query);

    const auto disp_start =
      std::max(0, std::min(n_filtered + legend_height - LINES,
                           std::max(0, cursor_pos - LINES / 2)));
    const auto disp_end = std::min(n_filtered, disp_start + LINES);

    const auto to_show = get_to_show(filtered, disp_start, disp_end);

    // Clear to prepare for redraw and display the legend
    erase();  // performs better than using clear();
    mvprintw(0, 0, current_legend);

    for (const auto [idx, entry] : std::views::enumerate(to_show)) {
      // Calculate the global data index
      const auto data_idx = disp_start + idx;
      const auto y_pos = static_cast<std::int32_t>(idx + legend_height);
      if (y_pos >= LINES)
        break;

      // color this item if it's among the selections
      if (selected_keys.contains(entry.first))
        attron(COLOR_PAIR(3));

      // highlight this item if it's at the cursor position
      if (data_idx == cursor_pos) {
        if (multi_add || multi_remove)
          attron(COLOR_PAIR(4));
        else
          attron(COLOR_PAIR(2));
      }

      // Display (key: paragraph) with horizontal range
      mvprintw(y_pos, 0, format_current_entry(entry, horiz_pos, COLS, margin));

      // Reset attribute
      attroff(COLOR_PAIR(2));
      attroff(COLOR_PAIR(3));
      attroff(COLOR_PAIR(4));
    }
    refresh();

    // Handle user input
    const auto ch = getch();
    if (ch == 'q') {
      if (confirm_quit(selected_keys))
        break;
    }
    else if (ch == escape_key_code) {  // ESC key to reset query
      query.clear();
      cursor_pos = 0;
    }
    else if (ch == KEY_RIGHT) {  // Scroll right
      // NOLINTBEGIN (cppcoreguidelines-narrowing-conversions)
      const std::int32_t width = std::size(filtered[cursor_pos].second);
      // NOLINTEND (cppcoreguidelines-narrowing-conversions)
      if (margin + width > COLS)
        horiz_pos = std::min(horiz_pos + 1, (margin + width) - COLS);
    }
    else if (ch == KEY_LEFT) {  // Scroll left
      horiz_pos = std::max(horiz_pos - 1, 0);
    }
    else if (ch == KEY_DOWN) {
      cursor_pos = (cursor_pos + 1) % n_filtered;
      if (multi_add)
        do_add(filtered, cursor_pos, selected_keys);
      if (multi_remove)
        do_remove(filtered, cursor_pos, selected_keys);
    }
    else if (ch == KEY_NPAGE) {
      std::int32_t max_down =
        std::min(cursor_pos + LINES - legend_height, n_filtered - 1);
      if (multi_add)
        for (std::int32_t i = cursor_pos; i < max_down; ++i)
          do_add(filtered, i, selected_keys);
      if (multi_remove)
        for (std::int32_t i = cursor_pos; i < max_down; ++i)
          do_remove(filtered, i, selected_keys);
      cursor_pos = max_down;
    }
    else if (ch == KEY_END) {
      if (multi_add)
        for (std::int32_t i = cursor_pos; i < n_filtered; ++i)
          do_add(filtered, i, selected_keys);
      if (multi_remove)
        for (std::int32_t i = cursor_pos; i < n_filtered; ++i)
          do_remove(filtered, i, selected_keys);
      cursor_pos = n_filtered - 1;
    }
    else if (ch == KEY_UP) {
      cursor_pos = (cursor_pos - 1 + n_filtered) % n_filtered;
      if (multi_add)
        do_add(filtered, cursor_pos, selected_keys);
      if (multi_remove)
        do_remove(filtered, cursor_pos, selected_keys);
    }
    else if (ch == KEY_PPAGE) {
      std::int32_t max_up = std::max(cursor_pos - (LINES - legend_height), 0);
      if (multi_add)
        for (std::int32_t i = cursor_pos; i >= max_up; --i)
          do_add(filtered, i, selected_keys);
      if (multi_remove)
        for (std::int32_t i = cursor_pos; i >= max_up; --i)
          do_remove(filtered, i, selected_keys);
      cursor_pos = max_up;
    }
    else if (ch == KEY_HOME) {
      if (multi_add)
        for (std::int32_t i = cursor_pos; i >= 0; --i)
          do_add(filtered, i, selected_keys);
      if (multi_remove)
        for (std::int32_t i = cursor_pos; i >= 0; --i)
          do_remove(filtered, i, selected_keys);
      cursor_pos = 0;
    }
    else if (ch == ' ' || ch == '\n') {  // Select/deselect current item
      do_select(filtered, cursor_pos, selected_keys);
    }
    else if (ch == 'c') {  // Clear selected keys
      selected_keys.clear();
    }
    else if (ch == 'v') {  // Display selected keys
      show_selected_keys(selected_keys);
    }
    else if (ch == 'a') {  // Toggle multi-select mode
      multi_add = !multi_add;
      multi_remove = false;
      if (multi_add)
        do_add(filtered, cursor_pos, selected_keys);
    }
    else if (ch == 'r') {  // Toggle multi-select mode
      multi_remove = !multi_remove;
      multi_add = false;
      if (multi_remove)
        do_remove(filtered, cursor_pos, selected_keys);
    }
    else if (ch == '/') {  // Start search query
      get_query(query, query_re);
      cursor_pos = 0;
    }
  }

  endwin();  // End ncurses session

  return selected_keys | std::ranges::to<std::vector>();
}

[[nodiscard]] auto
write_output(const auto &data, std::string &json_filename) -> std::error_code {
  std::ofstream out(json_filename);
  if (!out)
    return std::make_error_code(std::errc(errno));
  for (const auto &d : data)
    std::println(out, "{}", d);
  return {};
}

static auto
throwing_handler(int sig) {
  clear();
  refresh();
  endwin();
  throw std::runtime_error(
    std::format("Terminating (received signal: {})", sig));
}

static auto
throwing_handler_message([[maybe_unused]] int sig) {
  clear();
  refresh();
  endwin();
  throw std::runtime_error(std::format("Received user request to quit"));
}

/// Register all signals that could disrupt the curses and leave the
/// terminal in a bad state.
static auto
register_signals() {
  (void)std::signal(SIGINT, throwing_handler_message);
  (void)std::signal(SIGQUIT, throwing_handler_message);
  (void)std::signal(SIGTERM, throwing_handler_message);

  (void)std::signal(SIGKILL, throwing_handler);
  (void)std::signal(SIGABRT, throwing_handler);
  (void)std::signal(SIGSEGV, throwing_handler);
  (void)std::signal(SIGFPE, throwing_handler);
}

auto
command_select_main(int argc, char *argv[]) -> int {  // NOLINT
  static constexpr auto command = "select";
  static const auto usage =
    std::format("Usage: xfr {} [options]\n", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  std::string input_file;
  std::string genome_name;
  std::string output_file;
  std::string config_dir;

  namespace po = boost::program_options;

  po::options_description desc("Options");
  desc.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("genome,g", po::value(&genome_name)->required(), "use this genome (required)")
    ("output,o", po::value(&output_file)->required(), "output file (required)")
    ("input-file,i", po::value(&input_file), "specify an input file")
    ("config-dir,c", po::value(&config_dir), "specify a config directory")
    // clang-format on
    ;
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help") || argc == 1) {
      std::println("{}\n{}", about_msg, usage);
      desc.print(std::cout);
      std::println("\n{}", description_msg);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    std::println("{}\n{}", about_msg, usage);
    desc.print(std::cout);
    std::println("\n{}", description_msg);
    return EXIT_FAILURE;
  }

  if (!input_file.empty() && !config_dir.empty()) {
    std::println("Specify at most one of input-file or config-dir");
    return EXIT_FAILURE;
  }

  try {
    std::error_code error{};
    if (input_file.empty()) {
      transferase::client_config config;
      config.config_dir = config_dir;
      error = config.set_defaults();
      if (error)
        throw std::runtime_error(std::format(
          "Error setting client configuration: {}", error.message()));
      input_file = (std::filesystem::path{config.labels_dir} /
                    std::format("{}.json", genome_name))
                     .string();
    }

    const auto data = load_data(input_file, error);
    if (error)
      throw std::runtime_error(
        std::format("Error reading input {}: {}", input_file, error.message()));

    std::println("Number of items loaded: {}", std::size(data));
    std::print("Type 'g' then Enter to proceed. Any other key to exit. ");
    if (std::cin.get() != 'g') {
      std::println("Terminating on user request");
      return EXIT_SUCCESS;
    }
    // ADS: register the signals so a handler can properly reset the
    // terminal on exit
    register_signals();
    const auto selected = main_loop(data);
    if (!selected.empty())
      if (error = write_output(selected, output_file); error) {
        std::println("Error writing output {}: {}", output_file,
                     error.message());
        return EXIT_FAILURE;
      }
  }
  catch (std::runtime_error &e) {
    std::println("{}", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

#endif
