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
#include "macos_helper.hpp"
#include "utilities.hpp"

#include "CLI11/CLI11.hpp"
#include "nlohmann/json.hpp"

#include <ncurses.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>  // for std::isprint
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <print>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>  // for std::remove_cvref_t
#include <unordered_map>
#include <unordered_set>
#include <utility>  // for std::pair
#include <vector>

namespace xfr = transferase;

[[nodiscard]] auto
load_selected_groups(const std::string &json_filename)
  -> std::map<std::string, std::map<std::string, std::string>> {
  static constexpr auto err_msg_fmt = "Failed to parse file: {}. Check that "
                                      "the file was produced by xfr select.";
  std::ifstream in(json_filename);
  if (!in)
    throw std::runtime_error(
      std::format("Failed to open file: {}", json_filename));

  const nlohmann::json payload = nlohmann::json::parse(in, nullptr, false);
  if (payload.is_discarded())
    throw std::runtime_error(std::format(err_msg_fmt, json_filename));

  std::map<std::string, std::string> group_accn;
  try {
    group_accn = payload;
  }
  catch (const nlohmann::json::exception &_) {
    throw std::runtime_error(std::format(err_msg_fmt, json_filename));
  }

  std::map<std::string, std::map<std::string, std::string>> groups;
  for (const auto &[group, accn] : group_accn)
    groups[group.substr(0, group.find_last_of('_'))].emplace(group, accn);

  return groups;
}

// Ensure that 'groups' of methylomes specified by the user for loading as
// initial groups are valid for the given metadata. This might fail, for
// example, if the metadata changed since the user formed the groups, or if
// the information in the groups file was modified.
auto
validate_groups(const std::string &genome, const std::string &metadata_file,
                const auto &groups, const auto &data) {
  static constexpr auto msg_fmt =
    "Accession {} from group {} not among methylomes for {} in {}";
  std::unordered_set<std::string> accessions;
  for (const auto &d : data)
    accessions.emplace(d.accession);
  for (const auto &group : groups) {
    for (const auto &[_, accession] : group.second) {
      if (accessions.find(accession) == std::cend(accessions))
        throw std::runtime_error(
          std::format(msg_fmt, accession, group.first, genome, metadata_file));
    }
  }
}

struct meth_meta {
  static constexpr auto sep = " | ";
  static constexpr std::int32_t sep_size = 3;
  std::string accession;
  std::string label;
  std::string details;

  [[nodiscard]] auto
  detail_size() const -> std::int32_t {
    // ADS: the "+ 3" below is for the separator std::size(" | ");
    // NOLINTNEXTLINE (*-narrowing-conversions)
    return std::size(label) + std::size(details) + sep_size;
  }

  [[nodiscard]] auto
  format(const std::int32_t horiz_pos,
         const bool show_details) const -> std::string {
    // Form the label by appending the colon
    auto acn = std::format("{}: ", accession);
    auto sample_name = label;
    if (show_details)
      sample_name += std::format("{}{}", sep, details);
    // If the horizontal position within the sample would be larger than the
    // sample name, just return the label
    if (horiz_pos > std::ssize(sample_name))
      return acn;

    // Full line starting at the horizontal position in the sampel name
    return std::format("{}{}", acn, sample_name.substr(horiz_pos));
  }
};

[[nodiscard]] auto
load_data(const std::string &json_filename)
  -> std::map<std::string, std::vector<meth_meta>> {
  static constexpr auto err_msg_fmt =
    "Failed to parse file: {}. Ensure xfr config was run and succeeded.\n"
    "If an input file was specified, verify the file format.";

  std::ifstream in(json_filename);
  if (!in)
    throw std::runtime_error("Failed to open file: " + json_filename);

  const nlohmann::json payload = nlohmann::json::parse(in, nullptr, false);
  if (payload.is_discarded())
    throw std::runtime_error("Failed to parse file: " + json_filename);

  std::map<std::string, std::map<std::string, std::vector<std::string>>> data;

  try {
    data = payload;
  }
  catch (const nlohmann::json::exception &_) {
    throw std::runtime_error(std::format(err_msg_fmt, json_filename));
  }

  std::map<std::string, std::vector<meth_meta>> r;
  const auto to_vec_key_vals = [&](const auto &x) -> std::vector<meth_meta> {
    std::vector<meth_meta> v;
    for (const auto &u : x)
      if (!u.second.empty())
        v.emplace_back(u.first, u.second.front(), u.second.back());
    return v;
  };
  std::ranges::for_each(data, [&](const auto &d) {
    r.emplace(d.first, to_vec_key_vals(d.second));
  });
  return r;
}

static auto
mvprintw_wrap(const int y, const int x, const std::string &s) {
  // NOLINTNEXTLINE (*-vararg)
  const auto ret = mvprintw(y, x, "%s", s.substr(0, COLS - 1).data());
  if (ret != OK)
    throw std::runtime_error(
      std::format("Error updating display (writing: {})", s));
}

static auto
mvprintw_wrap(int y, const int x, const std::vector<std::string> &lines) {
  std::int32_t i = y;
  for (const auto &line : lines)
    mvprintw_wrap(i++, x, line);
}

[[nodiscard]] static inline auto
update_cursor_pos(const int ch, const std::int32_t cursor_pos,
                  const std::int32_t n_items,
                  const std::int32_t n_lines) -> std::int32_t {
  switch (ch) {
  case KEY_DOWN:
    return (cursor_pos + 1) % n_items;
  case KEY_UP:
    return (cursor_pos - 1 + n_items) % n_items;
  case KEY_NPAGE:
    return std::min(cursor_pos + n_lines, n_items - 1);
  case KEY_PPAGE:
    return std::max(cursor_pos - n_lines, 0);
  case KEY_END:
    return n_items - 1;
  case KEY_HOME:
    return 0;
  default:
    return cursor_pos;
  }
}

[[nodiscard]] static inline auto
get_elements_to_display(const auto &filtered, const auto disp_start,
                        const auto disp_end) {
  const auto i = std::cbegin(filtered);
  return std::ranges::subrange(i + disp_start, i + disp_end);
}

[[nodiscard]] static inline auto
format_current_entry(const auto &entry, const auto horiz_pos,
                     const bool show_details) {
  // Form the label by appending the colon
  auto label = std::format("{}: ", entry.accession);

  auto sample_name = entry.label;
  if (show_details)
    sample_name += std::format(" | {}", entry.details);

  // If the horizontal position within the sample would be larger than the
  // same name, just return the label
  if (horiz_pos > std::ssize(sample_name))
    return label;

  // Full line starting at the horizontal position in the sampel name
  return std::format("{}{}", label, sample_name.substr(horiz_pos));
}

static inline auto
do_select(const auto &filtered, const auto cursor_pos, auto &selected_keys) {
  const auto curr_fltr = std::cbegin(filtered) + cursor_pos;
  const auto itr = selected_keys.find(curr_fltr->accession);
  if (itr == std::cend(selected_keys))
    selected_keys.insert(curr_fltr->accession);
  else
    selected_keys.erase(itr);
}

static inline auto
do_select_group(const auto &filtered, const auto cursor_pos,
                auto &selected_groups) {
  const auto curr_fltr = std::cbegin(filtered) + cursor_pos;
  const auto itr = selected_groups.find(*curr_fltr);
  if (itr == std::cend(selected_groups))
    selected_groups.insert(*curr_fltr);
  else
    selected_groups.erase(itr);
}

static inline auto
do_add(const auto &filtered, const auto cursor_pos, auto &selected_keys) {
  const auto curr_fltr = std::cbegin(filtered) + cursor_pos;
  const auto itr = selected_keys.find(curr_fltr->accession);
  if (itr == std::cend(selected_keys))
    selected_keys.insert(curr_fltr->accession);
}

static inline auto
do_remove(const auto &filtered, const auto cursor_pos, auto &selected_keys) {
  const auto curr_fltr = std::cbegin(filtered) + cursor_pos;
  const auto itr = selected_keys.find(curr_fltr->accession);
  if (itr != std::cend(selected_keys))
    selected_keys.erase(curr_fltr->accession);
}

[[nodiscard]] static inline auto
get_display_start(const std::int32_t n_items, const std::int32_t n_lines,
                  const std::int32_t cursor_pos) -> std::int32_t {
  return std::max(
    0, std::min(n_items - n_lines, std::max(0, cursor_pos - n_lines / 2)));
}

static inline auto
show_selected_keys(const auto &selected_keys) {
  using std::string_literals::operator""s;
  static constexpr std::int32_t header_height = 1;
  static constexpr std::int32_t footer_height = 0;
  static constexpr auto escape_key_code = 27;
  static const auto header_line = "Selected keys. ESC to exit."s;
  clear();
  if (selected_keys.empty()) {
    mvprintw_wrap(0, 0, header_line);
    mvprintw_wrap(1, 0, "Empty selection."s);
    refresh();
    getch();  // any key will go back to list for selection
    return;
  }

  // NOLINTNEXTLINE (*-narrowing-conversions)
  const std::int32_t n_items = std::size(selected_keys);
  std::int32_t cursor_pos = 0;

  using input_type =
    typename std::remove_cvref_t<decltype(selected_keys)>::value_type;
  std::vector<input_type> data(std::size(selected_keys));
  std::int32_t idx = 0;
  for (const auto &k : selected_keys)
    data[idx++] = k;
  std::ranges::sort(data);

  while (true) {
    // define n_lines here in case terminal is resized
    const auto n_lines = LINES - header_height - footer_height;
    const auto disp_start = get_display_start(n_items, n_lines, cursor_pos);
    const auto disp_end = std::min(n_items, disp_start + n_lines);
    const auto display = get_elements_to_display(data, disp_start, disp_end);

    erase();  // performs better than using clear();
    mvprintw_wrap(0, 0, header_line);
    idx = 0;
    for (const auto &key : display) {
      const auto data_idx = disp_start + idx;  // global data index
      if (data_idx == cursor_pos)
        attron(COLOR_PAIR(2));
      mvprintw_wrap(header_height + idx, 0, key);
      if (data_idx == cursor_pos)
        attroff(COLOR_PAIR(2));
      ++idx;  // enumerate
    }
    refresh();

    const auto ch = getch();
    if (ch == escape_key_code)
      break;
    cursor_pos = update_cursor_pos(ch, cursor_pos, n_items, n_lines);
  }
}

[[nodiscard]] static inline auto
get_groupname(std::string &groupname) -> bool {
  static constexpr auto header_fmt = "use alphanumeric '.' '_' | "
                                     "enter to confirm | esc to cancel\n"
                                     "name: {}";
  static constexpr auto escape_key_code = 27;
  static constexpr auto enter_key_code = 10;

  const auto valid_char = [](const int x) {
    return x < std::numeric_limits<unsigned char>::max() &&
           (std::isalnum(x) || x == '.' || x == '_');
  };

  // if user aborts, the groupname will be reset to what was given
  const std::string original_groupname{groupname};

  while (true) {
    clear();
    mvprintw_wrap(0, 0, std::format(header_fmt, groupname));
    refresh();
    const int ch = getch();
    if (ch == escape_key_code) {
      groupname = original_groupname;
      return false;
    }
    else if (ch == enter_key_code)
      return true;
    else if (valid_char(ch))
      groupname += static_cast<char>(ch);
    else if (!groupname.empty() && (ch == KEY_BACKSPACE || ch == KEY_DC))
      groupname.pop_back();
  }
}

auto
make_named_group(
  const auto &selected_items, const std::string &default_group_name,
  std::map<std::string, std::map<std::string, std::string>> &groups) {
  using std::string_literals::operator""s;
  constexpr auto akr = " -- any key to resume";

  if (selected_items.empty()) {  // No selection, inform and continue
    erase();
    mvprintw_wrap(0, 0, "Selection is empty"s + akr);
    refresh();
    getch();
    return;
  }

  // start with default group name and fix it
  std::string group_name = default_group_name;
  std::ranges::replace(group_name, ' ', '_');
  std::uint32_t j = 0;
  for (std::uint32_t i = 0; i < std::size(group_name); ++i)
    if (group_name[i] != '_' || group_name[i - 1] != '_')
      group_name[j++] = group_name[i];
  group_name.resize(j);

  const bool name_ok = get_groupname(group_name);
  erase();
  if (!name_ok)
    mvprintw_wrap(0, 0, "Aborting group naming on user request "s + akr);
  else if (group_name.empty())
    mvprintw_wrap(0, 0, "Aborting group naming due to empty name "s + akr);
  else {
    // make names for all methylomes in the group
    std::vector<std::string> sorted_items(std::size(selected_items));
    std::int32_t idx = 0;
    for (const auto &item : selected_items)
      sorted_items[idx++] = item;
    std::ranges::sort(sorted_items);
    std::map<std::string, std::string> group;
    idx = 1;
    for (const auto &item : sorted_items)
      group.emplace(std::format("{}_{:0>5}", group_name, idx++), item);
    groups.emplace(group_name, group);
    mvprintw_wrap(0, 0, std::format("Formed group {}", group_name) + akr);
  }
  refresh();
  getch();
}

static inline auto
remove_from_group(const std::string &group_name, auto &data,
                  const std::int32_t cursor_pos, auto &group) {
  const auto &name = data[cursor_pos].first;
  const auto itr = group.find(name);
  if (itr == std::cend(group))
    throw std::runtime_error("failed to group member: " + name);

  // Remove the element from the group
  group.erase(itr);

  // Recreate the sequential data
  data.clear();
  int idx = 1;
  for (const auto &[k, v] : group) {
    const auto d = std::pair{std::format("{}_{:0>5}", group_name, idx++), v};
    data.emplace_back(d);
  }

  // Recreate the group
  group.clear();
  for (const auto &d : data)
    group.emplace(d);
}

static inline auto
show_group(const std::string &group_name, auto &group, const auto &info) {
  using std::string_literals::operator""s;
  static constexpr auto sep = " | ";
  static constexpr std::int32_t header_height = 1;
  static constexpr std::int32_t footer_height = 0;
  static constexpr auto escape_key_code = 27;
  static constexpr auto hdr_fmt =
    "group: {} | esc to exit | arrows to navigate | del to remove entry | "
    "d to toggle detail | item {}/{}";
  clear();

  // NOLINTNEXTLINE (*-narrowing-conversions)
  std::int32_t n_items = std::size(group);
  std::int32_t cursor_pos = 0;

  std::vector<std::pair<std::string, std::string>> data(std::size(group));
  std::int32_t idx = 0;
  for (const auto &elem : group)
    data[idx++] = elem;
  std::ranges::sort(data);

  std::int32_t display_mode = 0;  // 1 or 2
  std::int32_t horiz_pos = 0;
  std::int32_t current_line_width = 0;

  while (true) {
    // define n_lines here in case terminal is resized
    const auto n_lines = LINES - header_height - footer_height;
    const auto disp_start = get_display_start(n_items, n_lines, cursor_pos);
    const auto disp_end = std::min(n_items, disp_start + n_lines);
    const auto to_show = get_elements_to_display(data, disp_start, disp_end);

    erase();  // performs better than using clear();
    mvprintw_wrap(0, 0,
                  std::format(hdr_fmt, group_name, cursor_pos + 1, n_items));
    idx = 0;
    for (const auto &k : to_show) {
      const auto &alt_name = k.first;
      const auto &accession = k.second;
      const auto data_idx = disp_start + idx;  // global data index
      if (data_idx == cursor_pos)
        attron(COLOR_PAIR(2));
      const auto info_itr = info.find(accession);
      if (info_itr == std::cend(info))
        throw std::runtime_error("failed to find info for " + accession);
      std::string line = std::format("{}: {}", alt_name, accession);
      std::string extra_line;
      if (display_mode >= 1) {
        line += sep;
        extra_line += std::format("{}", info_itr->second.label);
      }
      if (display_mode >= 2)
        extra_line += std::format("{}{}", sep, info_itr->second.details);

      if (data_idx == cursor_pos) {
        // NOLINTNEXTLINE (*-narrowing-conversions)
        current_line_width = std::size(line) + std::size(extra_line);
        if (current_line_width >= COLS)
          extra_line = extra_line.substr(horiz_pos);
      }

      line += extra_line;

      mvprintw_wrap(header_height + idx, 0, line);
      attroff(COLOR_PAIR(2));
      ++idx;  // enumerate
    }
    refresh();

    // user input
    const auto ch = getch();
    if (ch == escape_key_code)
      break;

    if (ch == 'd') {
      horiz_pos = 0;
      display_mode = (display_mode + 1) % 3;
      continue;
    }

    if (ch == KEY_RIGHT) {  // Scroll right
      if (current_line_width + 2 > COLS)
        horiz_pos = std::min(horiz_pos + 1, current_line_width + 2 - COLS);
      continue;
    }

    if (ch == KEY_LEFT) {  // Scroll left
      horiz_pos = std::max(horiz_pos - 1, 0);
      continue;
    }

    if (ch == KEY_BACKSPACE || ch == KEY_DC) {
      horiz_pos = 0;
      if (n_items == 1) {
        clear();
        mvprintw_wrap(
          0, 0,
          "no empty groups -- remove group instead -- any key to resume"s);
        refresh();
        getch();  // any key will go back to list for selection
        continue;
      }
      remove_from_group(group_name, data, cursor_pos, group);
      n_items = std::max(0, n_items - 1);
      cursor_pos = std::min(n_items - 1, cursor_pos);
      continue;
    }

    const auto prev_pos = cursor_pos;
    cursor_pos = update_cursor_pos(ch, cursor_pos, n_items, n_lines);
    if (prev_pos != cursor_pos)
      horiz_pos = 0;
  }
}

static inline auto
no_groups_defined(const std::string &header_line) {
  clear();
  mvprintw_wrap(0, 0, header_line);
  mvprintw_wrap(1, 0, "No groups defined -- any key to resume");
  refresh();
  getch();
}

static inline auto
show_groups(auto &groups, const auto &info) {
  using std::string_literals::operator""s;
  static constexpr std::int32_t header_height = 1;
  static constexpr std::int32_t footer_height = 0;
  static constexpr auto escape_key_code = 27;
  static const auto header_line =
    "esc to exit | enter to view methylomes | r to rename"s;
  clear();
  if (groups.empty()) {
    no_groups_defined(header_line);
    return;
  }

  // NOLINTNEXTLINE (*-narrowing-conversions)
  std::int32_t n_items = std::size(groups);
  std::int32_t cursor_pos = 0;

  using input_type = typename std::remove_cvref_t<decltype(groups)>::key_type;
  std::vector<input_type> data(std::size(groups));
  std::int32_t idx = 0;
  for (const auto &k : groups)
    data[idx++] = k.first;

  while (true) {
    const auto n_lines = LINES - header_height - footer_height;
    const auto disp_start = std::max(
      0, std::min(n_items - n_lines, std::max(0, cursor_pos - n_lines / 2)));
    const auto disp_end = std::min(n_items, disp_start + n_lines);
    const auto to_show = get_elements_to_display(data, disp_start, disp_end);

    erase();  // performs better than using clear();
    mvprintw_wrap(0, 0,
                  header_line +
                    std::format(" | item {}/{}", cursor_pos + 1, n_items));
    idx = 0;
    for (const auto &group_name : to_show) {
      const auto data_idx = disp_start + idx;  // global data index
      if (data_idx == cursor_pos)
        attron(COLOR_PAIR(2));

      const auto itr = groups.find(group_name);
      if (itr == std::cend(groups))
        throw std::runtime_error("failed to find group " + group_name);

      const auto line =
        std::format("{}: {}", group_name, std::size(itr->second));
      mvprintw_wrap(header_height + idx, 0, line);

      attroff(COLOR_PAIR(2));
      ++idx;  // enumerate
    }
    refresh();

    // user input
    const auto ch = getch();
    if (ch == escape_key_code) {
      break;
    }

    if (ch == '\n') {  // View group
      const auto &name = data[cursor_pos];
      const auto itr = groups.find(name);
      if (itr == std::cend(groups))
        throw std::runtime_error(std::format("failed to find group {}", name));
      show_group(itr->first, itr->second, info);
      continue;
    }

    if (ch == 'r') {  // Rename group
      const auto &name = data[cursor_pos];
      const auto itr = groups.find(name);
      if (itr == std::cend(groups))
        throw std::runtime_error(std::format("failed to find group {}", name));

      std::vector<std::string> original_group;
      for (const auto &[_, accession] : itr->second)
        original_group.push_back(accession);
      std::string original_groupname = itr->first;
      groups.erase(itr);
      make_named_group(original_group, original_groupname, groups);

      data.clear();
      std::ranges::copy(groups | std::views::elements<0>,
                        std::back_inserter(data));
      continue;
    }

    if (ch == KEY_BACKSPACE || ch == KEY_DC) {  // delete group
      const auto &name = data[cursor_pos];
      const auto itr = groups.find(name);
      if (itr == std::cend(groups))
        throw std::runtime_error(std::format("failed to find group {}", name));
      groups.erase(itr);

      data.clear();
      std::ranges::copy(groups | std::views::elements<0>,
                        std::back_inserter(data));

      n_items = std::max(0, n_items - 1);

      if (n_items == 0) {
        no_groups_defined(header_line);
        break;
      }
      cursor_pos = std::min(n_items - 1, cursor_pos);

      continue;
    }

    cursor_pos = update_cursor_pos(ch, cursor_pos, n_items, n_lines);
  }
}

static inline auto
show_help() {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto keys = std::array{
    std::pair{"up"sv, "Move up one item (wrap at top)"sv},
    std::pair{"down"sv, "Move down one item (wrap at bottom)"sv},
    std::pair{"right"sv, "Scroll right"sv},
    std::pair{"left"sv, "Scroll left"sv},
    std::pair{"page up"sv, "Move up one page"sv},
    std::pair{"page down"sv, "Move down one page"sv},
    std::pair{"home"sv, "Move to start of list"sv},
    std::pair{"end"sv, "Move to end of list"sv},
    std::pair{"space"sv, "Select or deselect current item"sv},
    std::pair{"c"sv, "Clear current selections"sv},
    std::pair{"v"sv, "View selections"sv},
    std::pair{"V"sv, "View groups"sv},
    std::pair{"d"sv, "Toggle detailed view"sv},
    std::pair{"a"sv, "Toggle multi-add mode"sv},
    std::pair{"r"sv, "Toggle multi-remove mode"sv},
    std::pair{"s"sv, "Enter search phrase"sv},
    std::pair{"w"sv, "Write selections to file"sv},
    std::pair{"W"sv, "Write defined methylome groups to file"sv},
    std::pair{"g"sv, "Define and name a methylome group"sv},
    std::pair{"q"sv, "Quit"sv},
    std::pair{"ctrl-c"sv, "Quit without saving"sv},
    std::pair{"h"sv, "This message (any key to leave)"sv},
  };
  clear();
  mvprintw_wrap(0, 0, "Help for Interactive Commands");
  std::int32_t line_num = 2;
  for (const auto &kv : keys)
    mvprintw_wrap(line_num++, 0, std::format("{}: {}", kv.first, kv.second));
  refresh();
  getch();  // any key will go back to list for selection
}

static inline auto
get_query(std::string &query, std::regex &query_re) {
  static constexpr auto escape_key_code = 27;
  static constexpr auto enter_key_code = 10;
  while (true) {
    clear();
    mvprintw_wrap(0, 0, std::format("Search: {}", query));
    refresh();
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
  }
  query_re = std::regex(query, std::regex_constants::icase);
}

[[nodiscard]] static inline auto
get_filename(std::string &filename) -> bool {
  static constexpr auto header_fmt = "use alphanumeric '.' '_' '-' | "
                                     "enter to confirm | esc to cancel\n"
                                     "filename: {}";
  static constexpr auto escape_key_code = 27;
  static constexpr auto enter_key_code = 10;

  const auto valid_char = [](const int x) {
    return x < std::numeric_limits<unsigned char>::max() &&
           (std::isalnum(x) || x == '.' || x == '_' || x == '-');
  };

  // if user aborts, the filename will be reset to what was given
  const std::string original_filename{filename};

  while (true) {
    clear();
    mvprintw_wrap(0, 0, std::format(header_fmt, filename));
    refresh();
    const int ch = getch();
    if (ch == escape_key_code) {
      filename = original_filename;
      return false;
    }
    else if (ch == enter_key_code)
      return true;
    else if (valid_char(ch))
      filename += static_cast<char>(ch);
    else if (!filename.empty() && (ch == KEY_BACKSPACE || ch == KEY_DC))
      filename.pop_back();
  }
}

auto
write_output(const auto &data, std::string &outfile) {
  using std::string_literals::operator""s;
  constexpr auto akr = " -- any key to resume";
  if (data.empty()) {
    erase();
    mvprintw_wrap(0, 0, "No methylomes selected"s + akr);
  }
  else {
    const auto write_ok = get_filename(outfile);
    erase();
    if (!write_ok)
      mvprintw_wrap(0, 0, "Aborting save on user request"s + akr);
    else if (outfile.empty())
      mvprintw_wrap(0, 0, "Aborting save due to empty filename"s + akr);
    else {
      std::ofstream out(outfile);
      if (!out)
        throw std::runtime_error("error writing to file: " + outfile);
      std::ranges::for_each(data,
                            [&](const auto &d) { std::println(out, "{}", d); });
      mvprintw_wrap(0, 0, "Selection saved"s + akr);
    }
  }
  refresh();
  getch();
}

auto
write_groups(const auto &data, std::string &outfile) {
  using std::string_literals::operator""s;
  constexpr auto akr = " -- any key to resume";
  if (data.empty()) {
    erase();
    mvprintw_wrap(0, 0, "No groups selected"s + akr);
  }
  else {
    const auto write_ok = get_filename(outfile);
    erase();
    if (!write_ok)
      mvprintw_wrap(0, 0, "Aborting save on user request"s + akr);
    else if (outfile.empty())
      mvprintw_wrap(0, 0, "Aborting save due to empty filename"s + akr);
    else {
      // put all groups into one vector of pairs
      std::map<std::string, std::string> altname_name;
      for (const auto &g : data)
        std::ranges::copy(g.second,
                          std::inserter(altname_name, std::end(altname_name)));

      std::ofstream out(outfile);
      if (!out)
        throw std::runtime_error("error writing to file: " + outfile);

      static constexpr auto n_indent = 4;
      nlohmann::json json_data = altname_name;
      std::println(out, "{}", json_data.dump(n_indent));
      mvprintw_wrap(0, 0, "Selected groups saved"s + akr);
    }
  }
  refresh();
  getch();
}

static inline auto
write_groups_dialogue(
  const std::map<std::string, std::map<std::string, std::string>> &groups,
  std::string &outfile) {
  using std::string_literals::operator""s;
  static constexpr std::int32_t header_height = 1;
  static constexpr std::int32_t footer_height = 0;
  static constexpr auto escape_key_code = 27;
  static constexpr auto header_line =
    "enter to proceed | space to (un)select group | esc to exit | item {}/{}";
  if (groups.empty()) {
    no_groups_defined(header_line);
    return;
  }

  std::unordered_set<std::string> selected_groups;

  // NOLINTNEXTLINE (*-narrowing-conversions)
  const std::int32_t n_items = std::size(groups);
  std::int32_t cursor_pos = 0;

  std::vector<std::string> data;
  for (const auto &k : groups) {
    data.push_back(k.first);
    selected_groups.emplace(k.first);
  }
  clear();

  while (true) {
    const auto n_lines = LINES - header_height - footer_height;
    const auto disp_start = std::max(
      0, std::min(n_items - n_lines, std::max(0, cursor_pos - n_lines / 2)));
    const auto disp_end = std::min(n_items, disp_start + n_lines);
    const auto to_show = get_elements_to_display(data, disp_start, disp_end);

    erase();  // performs better than using clear();
    mvprintw_wrap(0, 0, std::format(header_line, cursor_pos + 1, n_items));
    std::int32_t idx = 0;
    for (const auto &k : to_show) {
      const auto data_idx = disp_start + idx;  // global data index
      if (data_idx == cursor_pos)
        attron(COLOR_PAIR(2));

      const auto itr = groups.find(k);
      if (itr == std::cend(groups))
        throw std::runtime_error(std::format("failed to find group {}", k));

      const auto selected = selected_groups.contains(k);
      const auto line = std::format("({}) {}: {}", selected ? 'x' : ' ', k,
                                    std::size(itr->second));
      mvprintw_wrap(header_height + idx, 0, line);

      attroff(COLOR_PAIR(2));
      ++idx;  // enumerate
    }
    refresh();

    // user input
    const auto ch = getch();
    if (ch == escape_key_code)
      break;

    if (ch == 'h') {  // show help
      show_help();
      continue;
    }

    if (ch == ' ') {  // select group
      do_select_group(data, cursor_pos, selected_groups);
      continue;
    }

    if (ch == '\n') {  // write the output
      std::map<std::string, std::map<std::string, std::string>> to_write;
      for (const auto &sel : selected_groups) {
        const auto itr = groups.find(sel);
        if (itr == std::cend(groups))
          throw std::runtime_error("Error saving groups");
        to_write.emplace(*itr);
      }
      write_groups(to_write, outfile);
      continue;
    }

    cursor_pos = update_cursor_pos(ch, cursor_pos, n_items, n_lines);
  }
}

[[nodiscard]] static inline auto
confirm_quit() -> bool {
  const auto message = "Quit? [y/n]";
  int confirmation{};
  while (std::tolower(confirmation) != 'y' &&
         std::tolower(confirmation) != 'n') {
    erase();
    mvprintw_wrap(0, 0, message);
    refresh();
    confirmation = getch();
  }
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

[[nodiscard]] static inline auto
in_multi_mode(const int multi_mode) -> bool {
  return multi_mode != 0;
}

[[nodiscard]] static inline auto
multi_add(const int multi_mode) -> bool {
  return multi_mode == 1;
}

static inline auto
toggle_multi_add(int &multi_mode, const std::vector<meth_meta> &filtered,
                 const std::int32_t cursor_pos,
                 std::unordered_set<std::string> &selected_keys) {
  multi_mode = (multi_mode == 0) ? 1 : 0;
  if (multi_add(multi_mode))
    do_add(filtered, cursor_pos, selected_keys);
}

[[nodiscard]] static inline auto
multi_remove(const int multi_mode) -> bool {
  return multi_mode == 2;
}

static inline auto
toggle_multi_remove(int &multi_mode, const std::vector<meth_meta> &filtered,
                    const std::int32_t cursor_pos,
                    std::unordered_set<std::string> &selected_keys) {
  multi_mode = (multi_mode == 0) ? 2 : 0;
  if (multi_remove(multi_mode))
    do_remove(filtered, cursor_pos, selected_keys);
}

auto
main_loop(
  // const std::vector<std::tuple<std::string, std::string, std::string>>
  // &data,
  const std::vector<meth_meta> &data,
  std::map<std::string, std::map<std::string, std::string>> &groups,
  std::string &filename) -> std::vector<std::string> {
  static constexpr auto extra_margin_space = 2;  // colon, space and safety?
  static constexpr auto escape_key_code = 27;
  static constexpr auto escape_delay{25};
  // lines to keep at top of display
  static constexpr std::int32_t legend_height{2};
  static constexpr auto legend1 =
    "h=Help q=Quit Nav=Arrow/PgUp/PgDn/Home/End Spc=Add/remove "
    "[{}/{}, selected={}]";
  static constexpr auto legend2 =
    "a/r=Toggle multi-Add/Remove, v/c=View/Clear selection, "
    "s/Esc=Search/Clear ";
  // margin must be max key width plus room
  const auto key_sizes = std::views::transform(
    data, [](const auto &s) -> std::int32_t { return std::size(s.accession); });
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
  use_default_colors();
  init_pair(2, COLOR_CYAN, -1);   // Highlighted current item
  init_pair(3, COLOR_GREEN, -1);  // Selected items
  init_pair(4, COLOR_BLUE, -1);   // Multiple selection mode active

  std::unordered_map<std::string, meth_meta> info;
  for (const auto &d : data)
    info.emplace(d.accession, d);

  bool show_details = false;  // show detailed sample info
  std::unordered_set<std::string> selected_keys;
  std::vector<std::string> queries;
  std::string query_update;
  std::regex query_re;

  int multi_mode = 0;

  std::int32_t horiz_pos = 0;
  std::int32_t cursor_pos = 0;
  std::vector<std::vector<meth_meta>> filtered;
  filtered.push_back(data);

  while (true) {
    if (!query_update.empty() &&
        (queries.empty() || query_update != queries.back())) {
      // Filter data based on the query
      const auto query_found = [&](const meth_meta &x) -> bool {
        // ADS: need to take care of upper/lower case here
        return std::regex_search(x.label, query_re) ||
               std::regex_search(x.details, query_re);
      };
      filtered.push_back({});
      assert(std::size(filtered) >= 2u);
      const auto &prev_filtered = filtered[std::size(filtered) - 2];
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
    const auto n_lines = LINES - legend_height;
    const auto disp_start = std::max(
      0, std::min(n_filtered - n_lines, std::max(0, cursor_pos - n_lines / 2)));
    const auto disp_end = std::min(n_filtered, disp_start + n_lines);

    const auto to_show =
      get_elements_to_display(filtered.back(), disp_start, disp_end);

    // Include the query in the legened if appropriate
    const std::string current_legend1 = std::format(
      legend1, cursor_pos + 1, n_filtered, std::size(selected_keys));
    const std::string current_legend2 =
      std::format(legend2, cursor_pos + 1, n_filtered,
                  std::size(selected_keys)) +
      format_queries(queries);

    // Clear to prepare for redraw and display the legend
    erase();  // performs better than using clear();

    mvprintw_wrap(0, 0, std::vector{current_legend1, current_legend2});

    // for (const auto [idx, entry] : std::views::enumerate(to_show)) {
    bool reset_color = false;
    std::int32_t idx = 0;  // ADS: need to compare with signed cursor_pos
    for (const auto &entry : to_show) {
      // Calculate the global data index
      const auto data_idx = disp_start + idx;
      const auto y_pos = idx + legend_height;
      assert(y_pos < LINES);

      // highlight this item if it's at the cursor position
      if (data_idx == cursor_pos) {
        if (in_multi_mode(multi_mode))
          attron(COLOR_PAIR(4));
        else
          attron(COLOR_PAIR(2));
        reset_color = true;
      }
      else if (selected_keys.contains(entry.accession)) {
        // color this item if it's among the selections
        attron(COLOR_PAIR(3));
        reset_color = true;
      }

      // Display (key: paragraph) with horizontal range
      mvprintw_wrap(y_pos, 0, entry.format(horiz_pos, show_details));

      // Reset attribute
      if (reset_color) {
        attroff(COLOR_PAIR(2));
        attroff(COLOR_PAIR(3));
        attroff(COLOR_PAIR(4));
      }

      ++idx;  // enumerate
    }
    refresh();

    // user input
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
      horiz_pos = 0;
    }
    else if (ch == KEY_RIGHT) {  // Scroll right
      const auto width = filtered.back()[cursor_pos].detail_size();
      if (margin + width + 1 > COLS)
        horiz_pos = std::min(horiz_pos + 1, (margin + width) + 1 - COLS);
    }
    else if (ch == KEY_LEFT) {  // Scroll left
      horiz_pos = std::max(horiz_pos - 1, 0);
    }
    else if (ch == 'h') {  // show help
      horiz_pos = 0;
      show_help();
    }
    else if (ch == KEY_DOWN) {
      horiz_pos = 0;
      cursor_pos = (cursor_pos + 1) % n_filtered;
      if (multi_add(multi_mode))
        do_add(filtered.back(), cursor_pos, selected_keys);
      if (multi_remove(multi_mode))
        do_remove(filtered.back(), cursor_pos, selected_keys);
    }
    else if (ch == KEY_NPAGE) {
      horiz_pos = 0;
      const auto max_down = std::min(cursor_pos + n_lines, n_filtered - 1);
      if (multi_add(multi_mode))
        for (auto i = cursor_pos; i < max_down; ++i)
          do_add(filtered.back(), i, selected_keys);
      if (multi_remove(multi_mode))
        for (auto i = cursor_pos; i < max_down; ++i)
          do_remove(filtered.back(), i, selected_keys);
      cursor_pos = max_down;
    }
    else if (ch == KEY_END) {
      horiz_pos = 0;
      if (multi_add(multi_mode))
        for (auto i = cursor_pos; i < n_filtered; ++i)
          do_add(filtered.back(), i, selected_keys);
      if (multi_remove(multi_mode))
        for (auto i = cursor_pos; i < n_filtered; ++i)
          do_remove(filtered.back(), i, selected_keys);
      cursor_pos = n_filtered - 1;
    }
    else if (ch == KEY_UP) {
      horiz_pos = 0;
      cursor_pos = (cursor_pos - 1 + n_filtered) % n_filtered;
      if (multi_add(multi_mode))
        do_add(filtered.back(), cursor_pos, selected_keys);
      if (multi_remove(multi_mode))
        do_remove(filtered.back(), cursor_pos, selected_keys);
    }
    else if (ch == KEY_PPAGE) {
      horiz_pos = 0;
      const auto max_up = std::max(cursor_pos - n_lines, 0);
      if (multi_add(multi_mode))
        for (auto i = cursor_pos; i >= max_up; --i)
          do_add(filtered.back(), i, selected_keys);
      if (multi_remove(multi_mode))
        for (auto i = cursor_pos; i >= max_up; --i)
          do_remove(filtered.back(), i, selected_keys);
      cursor_pos = max_up;
    }
    else if (ch == KEY_HOME) {
      horiz_pos = 0;
      if (multi_add(multi_mode))
        for (auto i = cursor_pos; i >= 0; --i)
          do_add(filtered.back(), i, selected_keys);
      if (multi_remove(multi_mode))
        for (auto i = cursor_pos; i >= 0; --i)
          do_remove(filtered.back(), i, selected_keys);
      cursor_pos = 0;
    }
    else if (ch == ' ') {  // Select/deselect current item
      do_select(filtered.back(), cursor_pos, selected_keys);
    }
    else if (ch == 'd') {  // Toggle detailed view
      show_details = !show_details;
    }
    else if (ch == 'c') {  // Clear selected keys
      selected_keys.clear();
    }
    else if (ch == 'v') {  // Display selected keys
      show_selected_keys(selected_keys);
    }
    else if (ch == 'w') {  // w key to save selection
      write_output(selected_keys, filename);
    }
    else if (ch == 'g') {  // g key to make a named group
      make_named_group(selected_keys, join_with(queries, '_'), groups);
    }
    else if (ch == 'W') {  // W key to save selected groups
      write_groups_dialogue(groups, filename);
    }
    else if (ch == 'V') {  // V key to display groups
      show_groups(groups, info);
    }
    else if (ch == 'a') {  // Toggle multi-add mode
      toggle_multi_add(multi_mode, filtered.back(), cursor_pos, selected_keys);
    }
    else if (ch == 'r') {  // Toggle multi-remove mode
      toggle_multi_remove(multi_mode, filtered.back(), cursor_pos,
                          selected_keys);
    }
    else if (ch == '/' || ch == 's') {  // Start search query
      horiz_pos = 0;
      cursor_pos = 0;
      get_query(query_update, query_re);
    }
  }  // end while (true)

  endwin();  // End ncurses session

  return selected_keys | std::ranges::to<std::vector>();
}

static auto
signal_handler(int sig) {
  clear();
  refresh();
  endwin();
  std::println("Terminating (received signal: {})", sig);
  exit(sig);
}

static auto
signal_handler_message([[maybe_unused]] int sig) {
  clear();
  refresh();
  endwin();
  std::println("Received user request to quit");
  exit(0);
}

/// Register all signals that could disrupt the curses and leave the
/// terminal in a bad state.
static auto
register_signals() {
  (void)std::signal(SIGINT, signal_handler_message);
  (void)std::signal(SIGQUIT, signal_handler_message);
  (void)std::signal(SIGTERM, signal_handler_message);

  (void)std::signal(SIGKILL, signal_handler);
  (void)std::signal(SIGABRT, signal_handler);
  (void)std::signal(SIGSEGV, signal_handler);
  (void)std::signal(SIGFPE, signal_handler);
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
  std::string selected_groups_file;

  CLI::App app{about_msg};
  argv = app.ensure_utf8(argv);
  app.usage(usage);
  if (argc >= 2)
    app.footer(description_msg);
  app.get_formatter()->column_width(column_width_default);
  app.get_formatter()->label("REQUIRED", "");
  app.set_help_flag("-h,--help", "Print a detailed help message and exit");
  // clang-format off
  app.add_option("-g,--genome", genome_name, "use this genome")->required();
  app.add_option("-s,--selected", selected_groups_file, "previously selected groups");
  app.add_option("-o,--output", output_file, "output file (you will be promoted before saving)");
  const auto input_file_opt =
    app.add_option("--input-file", input_file, "specify a non-default metadata input file")
    ->option_text("FILE")
    ->check(CLI::ExistingFile);
  app.add_option("-c,--config-dir", config_dir, "specify a non-default config directory")
    ->option_text("DIR")
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

    if (input_file.empty()) {
      std::error_code error{};
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
      input_file = config.get_select_metadata_file();
    }

    const auto all_data = load_data(input_file);

    const auto data_itr = all_data.find(genome_name);
    if (data_itr == std::cend(all_data)) {
      std::println("Data not found for genome: {}", genome_name);
      std::println("Available genomes are:");
      for (const auto &g : all_data)
        std::println("{}", g.first);
      return EXIT_FAILURE;
    }

    std::map<std::string, std::map<std::string, std::string>> groups;
    if (!selected_groups_file.empty()) {
      groups = load_selected_groups(selected_groups_file);
      validate_groups(genome_name, input_file, groups, data_itr->second);
    }

    std::println("Number of items loaded: {}", std::size(data_itr->second));
    std::print("Type 'g' then Enter to proceed. Any other key to exit. ");
    if (std::cin.get() != 'g') {
      std::println("Exiting on user request");
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
    main_loop(data_itr->second, groups, output_file);
  }
  catch (std::runtime_error &e) {
    std::println("{}", e.what());
    signal_handler(SIGTERM);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

#endif
