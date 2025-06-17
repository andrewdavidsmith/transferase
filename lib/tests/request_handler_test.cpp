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

#include <request_handler.hpp>

#include <genome_index.hpp>
#include <genomic_interval.hpp>
#include <level_container.hpp>
#include <logger.hpp>  // ADS: so we can setup the logger
#include <query_container.hpp>
#include <query_element.hpp>
#include <request.hpp>
#include <request_type_code.hpp>
#include <response.hpp>
#include <server.hpp>
#include <server_error_code.hpp>

#include <gtest/gtest.h>

#include <memory>  // std::unique_ptr
#include <string>
#include <system_error>
#include <vector>

#include <filesystem>
#include <format>
#include <iterator>  // for std::size
#include <unordered_map>

namespace transferase {
struct level_element_t;
}

using namespace transferase;  // NOLINT

TEST(request_handler_test, basic_assertions) {
  static constexpr auto mock_max_live_methylomes = 8u;
  request_handler rh("data", "data", mock_max_live_methylomes);
  EXPECT_EQ(rh.methylome_dir, "data");
  EXPECT_EQ(rh.index_file_dir, "data");
}

// Test fixture class for request_handler
class request_handler_mock : public ::testing::Test {
protected:
  void
  SetUp() override {
    // initialize the regular variables
    max_live_methylomes = 3;
    methylome_dir = "data/lutions/methylomes";
    index_file_dir = "data/lutions/indexes";
    raw_data_dir = "data/lutions/raw";

    logger::instance(shared_from_cout(), "command", log_level_t::debug);

    mock_methylome_set =
      std::make_unique<methylome_set>(methylome_dir, max_live_methylomes);
    mock_genome_index_set = std::make_unique<genome_index_set>(index_file_dir);
    mock_request_handler = std::make_unique<request_handler>(
      methylome_dir, index_file_dir, max_live_methylomes);
  }

  void
  TearDown() override {}

  // NOLINTBEGIN(*-non-private-member-variables-in-classes)
  std::uint32_t max_live_methylomes{};
  std::filesystem::path raw_data_dir;
  std::filesystem::path methylome_dir;
  std::filesystem::path index_file_dir;
  std::unique_ptr<methylome_set> mock_methylome_set;
  std::unique_ptr<genome_index_set> mock_genome_index_set;
  std::unique_ptr<request_handler> mock_request_handler;
  // NOLINTEND(*-non-private-member-variables-in-classes)
};

TEST_F(request_handler_mock, handle_request_success) {
  static constexpr auto rq_type = request_type_code::intervals;
  static constexpr auto n_intervals = 100u;
  static constexpr auto index_hash = 233205952u;
  static constexpr auto expected_rows = n_intervals;
  static constexpr auto expected_cols = 1u;

  [[maybe_unused]] const auto offsets =
    std::vector<transferase::query_element>(n_intervals);
  const request req{rq_type, index_hash, n_intervals, {"eFlareon_brain"}};
  response_header resp_hdr;
  mock_request_handler->handle_request(req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_error_code::ok);
  EXPECT_EQ(resp_hdr.rows, expected_rows);
  EXPECT_EQ(resp_hdr.cols, expected_cols);
  EXPECT_EQ(std::size(mock_request_handler->methylomes.accession_to_methylome),
            1u);
}

TEST_F(request_handler_mock, handle_request_bad_state) {
  static constexpr auto index_hash = 0u;
  static constexpr auto ok_accession = "eFlareon_brain";
  static constexpr auto malformed_accession = "eFlareon_..brain";
  static constexpr auto valid_rq_type = request_type_code::intervals;
  // ADS: (below) not a valid type
  static constexpr auto invalid_rq_type = static_cast<request_type_code>(6);
  static constexpr auto n_intervals = 100u;

  request req{valid_rq_type, index_hash, n_intervals, {malformed_accession}};

  response_header resp_hdr;
  mock_request_handler->handle_request(req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_error_code::invalid_methylome_name);

  req = request{invalid_rq_type, index_hash, n_intervals, {ok_accession}};
  mock_request_handler->handle_request(req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_error_code::invalid_request_type);
}

TEST_F(request_handler_mock, handle_request_failure) {
  static constexpr auto index_hash = 0u;
  static constexpr auto non_existent_accession{"eFlareon_brainZZZ"};
  static constexpr auto rq_type = request_type_code::intervals;
  static constexpr auto n_intervals = 100u;
  static constexpr auto expected_rows = 0u;  // due to error
  static constexpr auto expected_cols = 0u;  // due to error
  [[maybe_unused]] const auto offsets =
    std::vector<transferase::query_element>(n_intervals);

  request req{rq_type, index_hash, n_intervals, {"eFlareon_brain"}};
  response_header resp_hdr;
  req.methylome_names.front() = non_existent_accession;
  mock_request_handler->handle_request(req, resp_hdr);
  EXPECT_EQ(resp_hdr.status, server_error_code::methylome_not_found);
  EXPECT_EQ(resp_hdr.rows, expected_rows);
  EXPECT_EQ(resp_hdr.cols, expected_cols);
  EXPECT_EQ(std::size(mock_request_handler->methylomes.accession_to_methylome),
            0u);
}

TEST_F(request_handler_mock, intervals_get_levels_success) {
  // ADS: the index_hash below is taken from the data file
  static constexpr auto index_hash = 233205952u;
  static constexpr auto rq_type = request_type_code::intervals;
  static constexpr auto assembly = "eFlareon";
  static constexpr auto tissue = "brain";
  const auto methylome_name = std::format("{}_{}", assembly, tissue);
  const auto intervals_file = std::format("{}_hmr.bed", methylome_name);
  const auto intervals_path = raw_data_dir / intervals_file;
  const auto index_path = index_file_dir / std::format("{}.cpg_idx", assembly);

  EXPECT_TRUE(std::filesystem::exists(intervals_path));
  EXPECT_TRUE(std::filesystem::exists(index_path));

  std::error_code ec;
  const auto index = genome_index::read(index_file_dir, assembly, ec);
  EXPECT_FALSE(ec);

  const auto intervals = genomic_interval::read(index, intervals_path, ec);
  EXPECT_FALSE(ec);

  const auto query = index.make_query(intervals);

  request req{rq_type, index_hash, std::size(intervals), {methylome_name}};
  response_header resp_hdr;
  mock_request_handler->handle_request(req, resp_hdr);

  // ADS: payload stays on server side
  level_container<transferase::level_element_t> resp_data;
  mock_request_handler->intervals_get_levels<transferase::level_element_t>(
    req, query, resp_hdr, resp_data);

  const std::uint32_t req_offset_elem_size = sizeof(transferase::query_element);
  const std::uint32_t expected_payload_size =
    req_offset_elem_size * std::size(query);
  EXPECT_EQ(resp_data.get_n_bytes(), expected_payload_size);
}

TEST_F(request_handler_mock, bins_get_levels_success) {
  // ADS: the index_hash below is taken from the data file
  static constexpr auto index_hash = 233205952u;
  static constexpr auto bin_size = 100u;
  static constexpr auto rq_type = request_type_code::bins;
  static constexpr auto assembly = "eFlareon";
  static constexpr auto tissue = "brain";
  const auto methylome_name = std::format("{}_{}", assembly, tissue);
  const auto index_path =
    index_file_dir /
    std::format("{}{}", assembly, genome_index_data::filename_extension);

  EXPECT_TRUE(std::filesystem::exists(index_path));

  std::error_code ec;
  const auto index = genome_index::read(index_file_dir, assembly, ec);
  EXPECT_FALSE(ec);

  request req{rq_type, index_hash, bin_size, {methylome_name}};
  response_header resp_hdr;
  mock_request_handler->handle_request(req, resp_hdr);
  EXPECT_EQ(resp_hdr.rows, index.get_n_bins(bin_size)) << resp_hdr.summary();
  EXPECT_EQ(resp_hdr.cols, 1u) << resp_hdr.summary();

  level_container<level_element_t> resp_data;
  mock_request_handler->bins_get_levels<level_element_t>(req, resp_hdr,
                                                         resp_data);

  const auto expected_n_bins = index.get_n_bins(bin_size);

  const auto req_offset_elem_size = sizeof(transferase::query_element);
  const auto expected_payload_size = req_offset_elem_size * expected_n_bins;
  EXPECT_EQ(resp_data.get_n_bytes(), expected_payload_size);
}
