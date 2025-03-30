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

static constexpr auto about = R"(
select methylomes based on metadata related to biological samples
)";

static constexpr auto description = R"(
This command interacts with MethBase2 metadata files for experiments, allowing
methylomes to be selected based on information about the associated biological
samples. This command uses a text-based user interface with list navigation. A
genome must be specified because the selection can only be done for one genome
at a time. The selected methylomes are output to a text file with one
methylome accession per line. The purpose of this file is to serve as input
for transferase queries.
)";

static constexpr auto examples = R"(
Examples:

xfr select -o output_file.txt -g hg38
)";

#ifndef HAVE_NCURSES

#include <print>
auto
command_select_main([[maybe_unused]] int argc,
                    [[maybe_unused]] char *argv[])
  -> int {  // NOLINT(*-c-arrays)
  std::println("the 'select' command was not built");
  return EXIT_SUCCESS;
}

#else

#include "cli_common.hpp"
#include "client_config.hpp"
#include "nlohmann/json.hpp"
#include "utilities.hpp"

#include "CLI11/CLI11.hpp"

#include <ncurses.h>

#include <algorithm>
#include <array>
#include <cctype>  // for std::isprint
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>  // for std::getchar
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
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
  -> std::map<std::string, std::vector<std::pair<std::string, std::string>>> {
  std::ifstream in(json_filename);
  if (!in) {
    error = std::make_error_code(std::errc(errno));
    return {};
  }
  const nlohmann::json payload = nlohmann::json::parse(in, nullptr, false);
  if (payload.is_discarded()) {
    error = std::make_error_code(std::errc::invalid_argument);
    return {};
  }
  std::map<std::string, std::map<std::string, std::string>> data = payload;

  typedef std::vector<std::pair<std::string, std::string>> vec_key_val;
  std::map<std::string, vec_key_val> r;
  std::ranges::for_each(data, [&](const auto &d) {
    r.emplace(d.first, d.second | std::ranges::to<vec_key_val>());
  });
  return r;
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

[[nodiscard]] static inline auto
format_current_entry(const auto &entry, const auto horiz_pos, const auto max_x,
                     const auto margin) {
  if (horiz_pos > std::ssize(entry.second))
    return std::format("{}:", entry.first);
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
  mvprintw(0, 0, "Selected keys: "s);  // NOLINT (*-type-vararg)
  if (selected_keys.empty())
    mvprintw(1, 0, "Empty selection."s);  // NOLINT (*-type-vararg)
  else {
    const auto joined = selected_keys | std::views::join_with(',') |
                        std::ranges::to<std::string>();
    mvprintw(1, 0, joined);
  }
  refresh();
  getch();  // any key will go back to list for selection
}

static inline auto
show_help() {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto keys = std::array{
    std::pair{"Up arrow"sv, "Up one item (wrap at top)"sv},
    std::pair{"Down arrow"sv, "Down one item (wrap at bottom)"sv},
    std::pair{"Right arrow"sv, "Scroll right (if needed)"sv},
    std::pair{"Down arrow"sv, "Scroll left (if needed)"sv},
    std::pair{"Page up"sv, "Move up one page"sv},
    std::pair{"Page down"sv, "Move down one page"sv},
    std::pair{"Home"sv, "Move to the beginning of the list"sv},
    std::pair{"End"sv, "Move to the end of the list"sv},
    std::pair{"Space/Enter"sv, "Select or deselect current item"sv},
    std::pair{"c"sv, "Clear current selections"sv},
    std::pair{"v"sv, "View current selections"sv},
    std::pair{"a"sv, "Toggle multi-select mode"sv},
    std::pair{"r"sv, "Toggle multi-remove mode"sv},
    std::pair{"s"sv, "Enter search phrase"sv},
    std::pair{"w"sv, "Write selections to file"sv},
    std::pair{"q"sv, "Quit"sv},
    std::pair{"C-c"sv, "Quit without saving"sv},
    std::pair{"h"sv, "This message (any key to leave)"sv},
  };
  clear();
  std::int32_t line_num = 0;
  for (const auto &kv : keys) {
    const auto line = std::format("{}: {}", kv.first, kv.second);
    mvprintw(line_num++, 0, line);
  }
  refresh();
  getch();  // any key will go back to list for selection
}

static inline auto
get_query(std::string &query, std::regex &query_re) {
  static constexpr auto escape_key_code = 27;
  static constexpr auto enter_key_code = 10;
  clear();
  mvprintw(0, 0, std::format("Search Query: {}", query));
  refresh();
  while (true) {
    const auto query_ch = getch();
    if (query_ch == escape_key_code)
      break;  // ESC to cancel
    else if (query_ch == enter_key_code)
      break;  // Enter to submit
    else if (std::isprint(query_ch) || query_ch == ' ')
      query += static_cast<char>(query_ch);
    else if (query_ch == KEY_BACKSPACE || query_ch == KEY_DC) {
      if (!query.empty())
        query.pop_back();
    }
    clear();
    mvprintw(0, 0, std::format("Search Query: {}", query));
    refresh();
  }
  query_re = std::regex(query, std::regex_constants::icase);
}

static inline auto
get_filename(std::string &filename) {
  static constexpr auto escape_key_code = 27;
  static constexpr auto enter_key_code = 10;
  static constexpr auto header1 = "Delete and backspace to change. Use "
                                  "alphanumeric characters or '.', '_', '-'.";
  static constexpr auto header2 = "Enter to confirm, Escape to cancel";
  static constexpr auto msg_fmt = "Filename: {}";

  const auto valid_char_for_filename = [](const auto x) {
    return std::isalnum(x) || x == '.' || x == '_' || x == '-';
  };

  const std::string original_filename{filename};

  clear();
  mvprintw(0, 0, header1);  // NOLINT (*-type-vararg)
  mvprintw(1, 0, header2);  // NOLINT (*-type-vararg)
  mvprintw(2, 0, std::format(msg_fmt, filename));
  refresh();

  while (true) {
    const auto filename_ch = getch();
    if (filename_ch == escape_key_code) {
      filename = original_filename;
      break;  // ESC to cancel
    }
    else if (filename_ch == enter_key_code) {
      break;  // Enter to submit
    }
    else if (valid_char_for_filename(filename_ch)) {
      filename += static_cast<char>(filename_ch);
    }
    else if (filename_ch == KEY_BACKSPACE || filename_ch == KEY_DC) {
      if (!filename.empty())
        filename.pop_back();
    }

    clear();
    mvprintw(0, 0, header1);  // NOLINT (*-type-vararg)
    mvprintw(1, 0, header2);  // NOLINT (*-type-vararg)
    mvprintw(2, 0, std::format(msg_fmt, filename));
    refresh();
  }
}

auto
write_output(const auto &data, std::string &outfile) {
  constexpr auto msg_fmt1 = "Selected {} methylomes. Save to file: {}?";
  constexpr auto msg_fmt1_empty =
    "Selected {} methylomes. No filename specified.";
  constexpr auto msg_fmt2 = "y: accept, n: specify another filename, c: cancel";
  constexpr auto msg_fmt2_empty = "n: specify another filename, c: cancel";
  constexpr auto msg_fmt3 = "[Saving will not clear your selections]";
  int confirmation{};
  if (data.empty()) {
    const auto empty_sel_msg = "Selection is empty. Any key to return.";
    confirmation = '\0';
    while (confirmation != '\0') {
      erase();
      // NOLINTNEXTLINE (*-type-vararg)
      mvprintw(0, 0, empty_sel_msg);
      refresh();
      confirmation = std::getchar();
    }
    erase();
    mvprintw(0, 0, empty_sel_msg + std::string(" {}", confirmation));
    refresh();
    return;
  }

  const auto confirming_save = [](const auto x, const auto &filename) {
    const auto l = std::tolower(x);
    return (l != 'y' || filename.empty()) && l != 'c';
  };
  const auto awaiting_input = [](const auto x) {
    return std::tolower(x) != 'y' && std::tolower(x) != 'c' &&
           std::tolower(x) != 'n';
  };

  // A selection exists, apply the save dialogue
  confirmation = '\0';
  while (confirming_save(confirmation, outfile)) {
    confirmation = '\0';
    while (awaiting_input(confirmation)) {
      erase();
      if (outfile.empty()) {
        mvprintw(0, 0, std::format(msg_fmt1_empty, std::size(data)));
        mvprintw(1, 0, msg_fmt2_empty);  // NOLINT (*-type-vararg)
      }
      else {
        mvprintw(0, 0, std::format(msg_fmt1, std::size(data), outfile));
        mvprintw(1, 0, msg_fmt2);  // NOLINT (*-type-vararg)
      }
      mvprintw(2, 0, msg_fmt3);  // NOLINT (*-type-vararg)
      refresh();
      confirmation = std::getchar();
    }
    if (std::tolower(confirmation) == 'n')
      get_filename(outfile);
  }

  if (std::tolower(confirmation) == 'y') {
    std::ofstream out(outfile);
    if (!out) {
      const auto err = std::make_error_code(std::errc(errno));
      throw std::system_error(err, std::format("writing output {}", outfile));
    }
    for (const auto &d : data)
      std::println(out, "{}", d);

    const auto done_message = "Selection saved. Any key to return.";
    confirmation = '\0';
    while (confirmation != '\0') {
      erase();
      mvprintw(0, 0, done_message);  // NOLINT (*-type-vararg)
      refresh();
      confirmation = std::getchar();
    }
    erase();
    mvprintw(0, 0, done_message);  // NOLINT (*-type-vararg)
    refresh();
  }
}

[[nodiscard]] static inline auto
confirm_quit() -> bool {
  const auto message = "Quit? [y/n]";
  int confirmation{};
  while (std::tolower(confirmation) != 'y' &&
         std::tolower(confirmation) != 'n') {
    erase();
    mvprintw(0, 0, message);  // NOLINT (*-type-vararg)
    refresh();
    confirmation = std::getchar();
  }
  erase();
  mvprintw(0, 0, message);  // NOLINT (*-type-vararg)
  refresh();
  return (confirmation == 'y' || confirmation == 'Y');
}

[[nodiscard]] static inline auto
format_queries(const std::vector<std::string> &queries) -> std::string {
  if (queries.empty())
    return {};

  std::string s = std::format("[filters: \"{}\"", queries.front());
  for (auto i = 1u; i < std::size(queries); ++i)
    s += std::format(", \"{}\"", queries[i]);
  s += "]";
  return s;
}

auto
main_loop(const std::vector<std::pair<std::string, std::string>> &data,
          std::string &filename) -> std::vector<std::string> {
  static constexpr auto extra_margin_space = 2;  // colon and space
  static constexpr auto escape_key_code = 27;
  static constexpr auto escape_delay{25};
  // lines to keep at top of display
  static constexpr std::int32_t legend_height{2};
  static constexpr auto legend =
    "h=Help, q=Quit, Move: Arrows/PgUp/PgDn/Home/End Space=Add/remove "
    "[{}/{}, selected={}]\n"
    "a/r=Toggle multi-add/remove, v/c=View/clear selection, "
    "s/Esc=Enter/clear ";
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
  std::vector<std::string> queries;
  std::string query_update;
  std::regex query_re;
  bool multi_add = false;
  bool multi_remove = false;
  std::int32_t horiz_pos = 0;
  std::int32_t cursor_pos = 0;
  std::vector<std::vector<std::pair<std::string, std::string>>> filtered;
  filtered.push_back(data);

  while (true) {
    if (!query_update.empty() &&
        (queries.empty() || query_update != queries.back())) {
      // Filter data based on the query
      const auto query_found = [&](const auto &x) -> bool {
        // ADS: need to take care of upper/lower case here
        return std::regex_search(x.second, query_re);
      };
      filtered.push_back({});
      assert(std::size(filtered) >= 2u);
      const auto &prev_filtered = filtered[filtered.size() - 2];
      std::ranges::copy_if(prev_filtered, std::back_inserter(filtered.back()),
                           query_found);
      if (filtered.back().empty())
        filtered.pop_back();
      else
        queries.push_back(query_update);
      query_update.clear();
    }
    assert(std::size(filtered) == std::size(queries) + 1);

    // NOLINTNEXTLINE (*-narrowing-conversions)
    const std::int32_t n_filtered = std::size(filtered.back());

    // Include the query in the legened if appropriate
    const std::string current_legend =
      queries.empty() ? std::format(legend, cursor_pos + 1, n_filtered,
                                    std::size(selected_keys))
                      : std::format(legend, cursor_pos + 1, n_filtered,
                                    std::size(selected_keys)) +
                          format_queries(queries);

    const auto disp_start =
      std::max(0, std::min(n_filtered + legend_height - LINES,
                           std::max(0, cursor_pos - LINES / 2)));
    const auto disp_end = std::min(n_filtered, disp_start + LINES);

    const auto to_show = get_to_show(filtered.back(), disp_start, disp_end);

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
      if (confirm_quit())
        break;
    }
    else if (ch == escape_key_code) {  // ESC key to reset query
      if (!queries.empty()) {
        queries.pop_back();
        filtered.pop_back();
      }
      cursor_pos = 0;
    }
    else if (ch == 'w') {  // w key to save selection
      write_output(selected_keys, filename);
    }
    else if (ch == KEY_RIGHT) {  // Scroll right
      // NOLINTNEXTLINE (*-narrowing-conversions)
      const std::int32_t width = std::size(filtered.back()[cursor_pos].second);
      if (margin + width > COLS)
        horiz_pos = std::min(horiz_pos + 1, (margin + width) - COLS);
    }
    else if (ch == KEY_LEFT) {  // Scroll left
      horiz_pos = std::max(horiz_pos - 1, 0);
    }
    else if (ch == KEY_DOWN) {
      horiz_pos = 0;
      cursor_pos = (cursor_pos + 1) % n_filtered;
      if (multi_add)
        do_add(filtered.back(), cursor_pos, selected_keys);
      if (multi_remove)
        do_remove(filtered.back(), cursor_pos, selected_keys);
    }
    else if (ch == KEY_NPAGE) {
      horiz_pos = 0;
      std::int32_t max_down =
        std::min(cursor_pos + LINES - legend_height, n_filtered - 1);
      if (multi_add)
        for (std::int32_t i = cursor_pos; i < max_down; ++i)
          do_add(filtered.back(), i, selected_keys);
      if (multi_remove)
        for (std::int32_t i = cursor_pos; i < max_down; ++i)
          do_remove(filtered.back(), i, selected_keys);
      cursor_pos = max_down;
    }
    else if (ch == KEY_END) {
      horiz_pos = 0;
      if (multi_add)
        for (std::int32_t i = cursor_pos; i < n_filtered; ++i)
          do_add(filtered.back(), i, selected_keys);
      if (multi_remove)
        for (std::int32_t i = cursor_pos; i < n_filtered; ++i)
          do_remove(filtered.back(), i, selected_keys);
      cursor_pos = n_filtered - 1;
    }
    else if (ch == KEY_UP) {
      horiz_pos = 0;
      cursor_pos = (cursor_pos - 1 + n_filtered) % n_filtered;
      if (multi_add)
        do_add(filtered.back(), cursor_pos, selected_keys);
      if (multi_remove)
        do_remove(filtered.back(), cursor_pos, selected_keys);
    }
    else if (ch == KEY_PPAGE) {
      horiz_pos = 0;
      std::int32_t max_up = std::max(cursor_pos - (LINES - legend_height), 0);
      if (multi_add)
        for (std::int32_t i = cursor_pos; i >= max_up; --i)
          do_add(filtered.back(), i, selected_keys);
      if (multi_remove)
        for (std::int32_t i = cursor_pos; i >= max_up; --i)
          do_remove(filtered.back(), i, selected_keys);
      cursor_pos = max_up;
    }
    else if (ch == KEY_HOME) {
      horiz_pos = 0;
      if (multi_add)
        for (std::int32_t i = cursor_pos; i >= 0; --i)
          do_add(filtered.back(), i, selected_keys);
      if (multi_remove)
        for (std::int32_t i = cursor_pos; i >= 0; --i)
          do_remove(filtered.back(), i, selected_keys);
      cursor_pos = 0;
    }
    else if (ch == ' ' || ch == '\n') {  // Select/deselect current item
      do_select(filtered.back(), cursor_pos, selected_keys);
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
        do_add(filtered.back(), cursor_pos, selected_keys);
    }
    else if (ch == 'r') {  // Toggle multi-select mode
      multi_remove = !multi_remove;
      multi_add = false;
      if (multi_remove)
        do_remove(filtered.back(), cursor_pos, selected_keys);
    }
    else if (ch == '/' || ch == 's') {  // Start search query
      horiz_pos = 0;
      get_query(query_update, query_re);
      cursor_pos = 0;
    }
    else if (ch == 'h') {  // show help
      horiz_pos = 0;
      show_help();
    }
  }

  endwin();  // End ncurses session

  return selected_keys | std::ranges::to<std::vector>();
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
command_select_main(int argc,
                    char *argv[]) -> int {  // NOLINT(*-c-arrays)
  static constexpr auto command = "select";
  static const auto usage =
    std::format("Usage: xfr {} [options]", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  std::string input_file;
  std::string genome_name;
  std::string output_file;
  std::string config_dir;

  CLI::App app{about_msg};
  argv = app.ensure_utf8(argv);
  app.usage(usage);
  if (argc >= 2)
    app.footer(description_msg);
  app.get_formatter()->column_width(column_width_default);
  app.get_formatter()->label("REQUIRED", "REQD");
  app.set_help_flag("-h,--help", "Print a detailed help message and exit");
  // clang-format off
  app.add_option("-g,--genome", genome_name, "use this genome")->required();
  app.add_option("-o,--output", output_file, "output file (you will be promoted before saving)");
  const auto input_file_opt =
    app.add_option("-i,--input-file", input_file, "specify an input file")
    ->option_text("TEXT:FILE")
    ->check(CLI::ExistingFile);
  app.add_option("-c,--config-dir", config_dir, "specify a config directory")
    ->option_text("TEXT:DIR")
    ->check(CLI::ExistingDirectory)
    ->excludes(input_file_opt);
  // clang-format on

  if (argc < 2) {
    std::println("{}", app.help());
    return EXIT_SUCCESS;
  }

  CLI11_PARSE(app, argc, argv);

  try {
    using transferase::client_config;

    std::error_code error{};
    if (input_file.empty()) {
      if (config_dir.empty()) {
        config_dir = client_config::get_default_config_dir(error);
        if (error)
          throw std::runtime_error(std::format(
            "Error setting client configuration: {}", error.message()));
      }
      const auto config = client_config::read(config_dir, error);
      if (error)
        throw std::runtime_error(std::format("Error reading config dir {}: {}",
                                             config_dir, error.message()));
      input_file = config.get_labels_file();
    }

    const auto all_data = load_data(input_file, error);
    if (error)
      throw std::runtime_error(
        std::format("Error reading input {}: {}", input_file, error.message()));

    const auto data_itr = all_data.find(genome_name);
    if (data_itr == std::cend(all_data)) {
      std::println("Data not found for genome: {}", genome_name);
      std::println("Available genomes are:");
      for (const auto &g : all_data)
        std::println("{}", g.first);
      return EXIT_FAILURE;
    }

    std::println("Number of items loaded: {}", std::size(data_itr->second));
    std::print("Type 'g' then Enter to proceed. Any other key to exit. ");
    if (std::cin.get() != 'g') {
      std::println("Terminating on user request");
      return EXIT_SUCCESS;
    }
    // ADS: register the signals so a handler can properly reset the
    // terminal on exit
    register_signals();
#ifdef USE_XTERM
    const auto set_term_result = setenv("TERM", "xterm", 1);
    if (set_term_result) {
      const auto set_term_err = std::make_error_code(std::errc(errno));
      std::println("Error setting TERM env variable: {}",
                   set_term_err.message());
      return EXIT_FAILURE;
    }
#endif
    main_loop(data_itr->second, output_file);
  }
  catch (std::runtime_error &e) {
    std::println("{}", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

#endif
