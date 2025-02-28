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

#include <level_container_md.hpp>

#include <bins_writer.hpp>
#include <genome_index.hpp>
#include <genomic_interval.hpp>
#include <intervals_writer.hpp>
#include <level_container.hpp>
#include <level_element.hpp>

#include "unit_test_utils.hpp"

#include <gtest/gtest.h>

#include <cstdint>  // for std::uint32_t
#include <format>
#include <fstream>
#include <iterator>  // for std::size
#include <memory>    // for std::unique_ptr, std::shared_ptr
#include <ranges>    // for std::views
#include <stdexcept>
#include <string>
#include <tuple>  // for std::get
#include <unordered_map>
#include <vector>

// Tasks: round-trip file IO with level_container_md

// Consider slicing

// Conversion to and from the Python ndarray

// Is there any need to consider spans? Stide? Transpose?

using namespace transferase;  // NOLINT

TEST(level_container_md_test, default_constructor) {
  level_container_md<level_element_t> container;
  EXPECT_EQ(container.n_rows, 0);
  EXPECT_EQ(container.n_cols, 0);
  EXPECT_TRUE(container.v.empty());
}

TEST(level_container_md_test, parameterized_constructor) {
  constexpr std::uint32_t n_rows = 2;
  constexpr std::uint32_t n_cols = 3;
  level_container_md<level_element_t> container(2, 3);
  EXPECT_EQ(container.n_rows, n_rows);
  EXPECT_EQ(container.n_cols, n_cols);
  EXPECT_EQ(std::size(container), n_rows * n_cols);
}

TEST(level_container_md_test, vector_constructor) {
  constexpr std::uint32_t n_rows = 3;
  constexpr std::uint32_t n_cols = 1;
  auto vec = std::vector{
    level_element_t{1, 2},
    level_element_t{3, 4},
    level_element_t{5, 6},
  };
  level_container_md<level_element_t> container(std::move(vec));
  EXPECT_EQ(container.n_rows, n_rows);
  EXPECT_EQ(container.n_cols, n_cols);
  EXPECT_EQ(container.v.size(), n_rows * n_cols);
}

class level_container_md_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    container = level_container_md<level_element_t>(n_rows, n_cols);
    container[0, 0] = level_element_t{1, 2};
    container[0, 1] = level_element_t{3, 4};
    container[1, 0] = level_element_t{5, 6};
    container[1, 1] = level_element_t{7, 8};
  }

  auto
  TearDown() -> void override {}

public:
  level_container_md<level_element_t> container;
  const std::string expected_str = "1\t2\t3\t4\n5\t6\t7\t8\n";
  const std::uint32_t n_rows = 2;
  const std::uint32_t n_cols = 2;
};

TEST_F(level_container_md_mock, access_operator) {
  EXPECT_EQ((container[0, 0]), level_element_t(1, 2));
  EXPECT_EQ((container[0, 1]), level_element_t(3, 4));
  EXPECT_EQ((container[1, 0]), level_element_t(5, 6));
  EXPECT_EQ((container[1, 1]), level_element_t(7, 8));
}

TEST_F(level_container_md_mock, resize) {
  constexpr std::uint32_t new_size = 10;
  container.resize(new_size);
  EXPECT_EQ(std::size(container), new_size);
}

TEST_F(level_container_md_mock, reserve) {
  constexpr std::uint32_t new_capacity = 10;
  container.reserve(new_capacity);
  EXPECT_LE(std::size(container), new_capacity);
}

TEST_F(level_container_md_mock, get_n_bytes) {
  EXPECT_EQ(container.get_n_bytes(), sizeof(level_element_t) * n_rows * n_cols);
}

TEST_F(level_container_md_mock, data_methods) {
  const auto data_ptr = container.data();
  EXPECT_NE(data_ptr, nullptr);
  const auto const_data_ptr =
    static_cast<const level_container_md<level_element_t> &>(container).data();
  EXPECT_NE(const_data_ptr, nullptr);
}

TEST_F(level_container_md_mock, tostring) {
  const std::string s = container.tostring();
  EXPECT_EQ(s, expected_str) << s;
}

TEST_F(level_container_md_mock, roundtrip_test) {
  const auto tmp_filename = generate_temp_filename("tmp");
  std::ofstream out(tmp_filename);
  if (!out)
    throw std::runtime_error(
      std::format("failed to open temp output file: {}", tmp_filename));
  out << container.tostring();
  out.close();

  std::error_code error;
  const auto from_file = read_level_container_md(tmp_filename, error);
  EXPECT_FALSE(error) << error.message();

  remove_file(tmp_filename, error);
  EXPECT_FALSE(error);
}

TEST_F(level_container_md_mock, write_with_intervals_writer_test) {
  using std::literals::string_literals::operator""s;
  const std::uint32_t min_reads = 0;
  const auto methylomes_names = std::vector{
    "one"s,
    "two"s,
  };
  const auto intervals = std::vector{
    genomic_interval{0, 1000, 2000},
    genomic_interval{0, 3000, 4000},
  };
  // make a minimal sufficient genome index
  genome_index index;
  index.meta.chrom_order = std::vector{
    "chr1"s,
  };

  // make an equivalent vector of level_container
  using cont_type = level_container<level_element_t>;
  auto vec = std::vector<cont_type>(n_cols);
  for (auto c = 0u; c < n_cols; ++c) {
    vec[c] = cont_type(n_rows);
    for (auto r = 0u; r < n_rows; ++r)
      vec[c][r] = container[r, c];
  }

  std::error_code error;

  // do bedlike

  auto tmp_filename = generate_temp_filename("tmp_md_c");
  // clang-format off
  const auto writer_bedlike = intervals_writer{
    tmp_filename,
    index,
    output_format_t::counts,
    methylomes_names,
    min_reads,
    intervals,
  };
  // clang-format on

  // write the level_container_md
  std::error_code write_container_err =
    writer_bedlike.write_bedlike(container, true);
  EXPECT_FALSE(write_container_err) << write_container_err.message();

  // write the vector of level_container
  auto tmp_filename_vec = generate_temp_filename("tmp_vec_c");
  // clang-format off
  const auto writer_bedlike_for_vec = intervals_writer{
    tmp_filename_vec,
    index,
    output_format_t::counts,
    methylomes_names,
    min_reads,
    intervals,
  };
  // clang-format on
  std::error_code write_vec_err =
    writer_bedlike_for_vec.write_bedlike(vec, true);
  EXPECT_FALSE(write_vec_err) << write_vec_err.message();

  EXPECT_TRUE(files_are_identical(tmp_filename, tmp_filename_vec));
  remove_file(tmp_filename, error);
  EXPECT_FALSE(error);
  remove_file(tmp_filename_vec, error);
  EXPECT_FALSE(error);

  // do dataframe

  tmp_filename = generate_temp_filename("tmp_md_d");
  // clang-format off
  const auto writer_dataframe = intervals_writer{
    tmp_filename,
    index,
    output_format_t::dataframe,
    methylomes_names,
    min_reads,
    intervals,
  };
  // clang-format on

  // write the level_container_md
  write_container_err = writer_dataframe.write_dataframe(container);
  EXPECT_FALSE(write_container_err) << write_container_err.message();

  // write the vector of level_container
  tmp_filename_vec = generate_temp_filename("tmp_vec_d");
  // clang-format off
  const auto writer_dataframe_for_vec = intervals_writer{
    tmp_filename_vec,
    index,
    output_format_t::dataframe,
    methylomes_names,
    min_reads,
    intervals,
  };
  // clang-format on
  write_vec_err = writer_dataframe_for_vec.write_dataframe(vec);
  EXPECT_FALSE(write_vec_err) << write_vec_err.message();

  EXPECT_TRUE(files_are_identical(tmp_filename, tmp_filename_vec));
  remove_file(tmp_filename, error);
  EXPECT_FALSE(error);
  remove_file(tmp_filename_vec, error);
  EXPECT_FALSE(error);

  // do dataframe scores

  tmp_filename = generate_temp_filename("tmp_md_s");
  // clang-format off
  const auto writer_dataframe_scores = intervals_writer{
    tmp_filename,
    index,
    output_format_t::dataframe_scores,
    methylomes_names,
    min_reads,
    intervals,
  };
  // clang-format on

  // write the level_container_md
  write_container_err =
    writer_dataframe_scores.write_dataframe_scores(container);
  EXPECT_FALSE(write_container_err) << write_container_err.message();

  // write the vector of level_container
  tmp_filename_vec = generate_temp_filename("tmp_vec_s");
  // clang-format off
  const auto writer_dataframe_scores_for_vec = intervals_writer{
    tmp_filename_vec,
    index,
    output_format_t::dataframe_scores,
    methylomes_names,
    min_reads,
    intervals,
  };
  // clang-format on
  write_vec_err = writer_dataframe_scores_for_vec.write_dataframe_scores(vec);
  EXPECT_FALSE(write_vec_err) << write_vec_err.message();

  EXPECT_TRUE(files_are_identical(tmp_filename, tmp_filename_vec));
  remove_file(tmp_filename, error);
  EXPECT_FALSE(error);
  remove_file(tmp_filename_vec, error);
  EXPECT_FALSE(error);
}

TEST_F(level_container_md_mock, write_with_bins_writer_test) {
  using std::literals::string_literals::operator""s;
  const std::uint32_t bin_size = 2;
  const std::uint32_t min_reads = 1;
  const auto methylomes_names = std::vector{
    "one"s,
    "two"s,
  };
  // make a minimal sufficient genome index
  genome_index index;
  index.meta.chrom_order = std::vector{
    "chr1"s,
  };
  index.meta.chrom_size.push_back(4);

  // make an equivalent vector of level_container
  using cont_type = level_container<level_element_t>;
  auto vec = std::vector<cont_type>(n_cols);
  for (auto c = 0u; c < n_cols; ++c) {
    vec[c] = cont_type(n_rows);
    for (auto r = 0u; r < n_rows; ++r)
      vec[c][r] = container[r, c];
  }

  std::error_code error;

  // do bedlike

  auto tmp_filename = generate_temp_filename("tmp_md_c");
  // clang-format off
  const auto writer_bedlike = bins_writer{
    tmp_filename,
    index,
    output_format_t::counts,
    methylomes_names,
    min_reads,
    bin_size,
  };
  // clang-format on

  // write the level_container_md
  std::error_code write_container_err =
    writer_bedlike.write_bedlike(container, true);
  EXPECT_FALSE(write_container_err) << write_container_err.message();

  // write the vector of level_container
  auto tmp_filename_vec = generate_temp_filename("tmp_vec_c");
  // clang-format off
  const auto writer_bedlike_for_vec = bins_writer{
    tmp_filename_vec,
    index,
    output_format_t::counts,
    methylomes_names,
    min_reads,
    bin_size,
  };
  // clang-format on
  std::error_code write_vec_err =
    writer_bedlike_for_vec.write_bedlike(vec, true);
  EXPECT_FALSE(write_vec_err) << write_vec_err.message();

  EXPECT_TRUE(files_are_identical(tmp_filename, tmp_filename_vec));
  remove_file(tmp_filename, error);
  EXPECT_FALSE(error);
  remove_file(tmp_filename_vec, error);
  EXPECT_FALSE(error);

  // do dataframe

  tmp_filename = generate_temp_filename("tmp_md_d");
  // clang-format off
  const auto writer_dataframe = bins_writer{
    tmp_filename,
    index,
    output_format_t::dataframe,
    methylomes_names,
    min_reads,
    bin_size,
  };
  // clang-format on

  // write the level_container_md
  write_container_err = writer_dataframe.write_dataframe(container);
  EXPECT_FALSE(write_container_err) << write_container_err.message();

  // write the vector of level_container
  tmp_filename_vec = generate_temp_filename("tmp_vec_d");
  // clang-format off
  const auto writer_dataframe_for_vec = bins_writer{
    tmp_filename_vec,
    index,
    output_format_t::dataframe,
    methylomes_names,
    min_reads,
    bin_size,
  };
  // clang-format on
  write_vec_err = writer_dataframe_for_vec.write_dataframe(vec);
  EXPECT_FALSE(write_vec_err) << write_vec_err.message();

  EXPECT_TRUE(files_are_identical(tmp_filename, tmp_filename_vec));
  remove_file(tmp_filename, error);
  EXPECT_FALSE(error);
  remove_file(tmp_filename_vec, error);
  EXPECT_FALSE(error);

  // do dataframe scores

  tmp_filename = generate_temp_filename("tmp_md_s");
  // clang-format off
  const auto writer_dataframe_scores = bins_writer{
    tmp_filename,
    index,
    output_format_t::dataframe_scores,
    methylomes_names,
    min_reads,
    bin_size,
  };
  // clang-format on

  // write the level_container_md
  write_container_err =
    writer_dataframe_scores.write_dataframe_scores(container);
  EXPECT_FALSE(write_container_err) << write_container_err.message();

  // write the vector of level_container
  tmp_filename_vec = generate_temp_filename("tmp_vec_s");
  // clang-format off
  const auto writer_dataframe_scores_for_vec = bins_writer{
    tmp_filename_vec,
    index,
    output_format_t::dataframe_scores,
    methylomes_names,
    min_reads,
    bin_size,
  };
  // clang-format on
  write_vec_err = writer_dataframe_scores_for_vec.write_dataframe_scores(vec);
  EXPECT_FALSE(write_vec_err) << write_vec_err.message();

  EXPECT_TRUE(files_are_identical(tmp_filename, tmp_filename_vec));
  remove_file(tmp_filename, error);
  EXPECT_FALSE(error);
  remove_file(tmp_filename_vec, error);
  EXPECT_FALSE(error);
}

TEST(level_container_md_test, add_column_test) {
  const auto col_to_add = std::vector{
    level_element_t{3, 4},
    level_element_t{7, 8},
  };
  constexpr std::uint32_t n_rows = 2;
  constexpr std::uint32_t n_cols = 1;
  level_container_md<level_element_t> container(n_rows, n_cols);
  container[0, 0] = level_element_t{1, 2};
  container[1, 0] = level_element_t{5, 6};
  container.add_column(col_to_add);
  EXPECT_EQ(std::size(container), n_rows * (n_cols + 1))
    << container.tostring();
}
