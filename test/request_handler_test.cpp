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
#include <logger.hpp>  // ADS: so we can setup the logger
#include <methylome_data.hpp>
#include <methylome_metadata.hpp>
#include <query_container.hpp>
#include <query_element.hpp>
#include <request.hpp>
#include <request_type_code.hpp>
#include <response.hpp>
#include <server.hpp>

#include <gtest/gtest.h>

#include <memory>  // std::unique_ptr
#include <string>
#include <system_error>
#include <vector>

#include <filesystem>
#include <format>
#include <iterator>  // for std::size
#include <tuple>     // for std::ignore
#include <unordered_map>

using namespace transferase;  // NOLINT

TEST(request_handler_test, basic_assertions) {
  std::error_code ec;
  request_handler rh("data", "data", 8);
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

    logger::instance(shared_from_cout(), "command", log_level_t::critical);

    mock_methylome_set =
      std::make_unique<methylome_set>(methylome_dir, max_live_methylomes);
    mock_genome_index_set = std::make_unique<genome_index_set>(index_file_dir);
    mock_request_handler = std::make_unique<request_handler>(
      methylome_dir, index_file_dir, max_live_methylomes);
  }

  void
  TearDown() override {}

  std::uint32_t max_live_methylomes{};
  std::filesystem::path raw_data_dir;
  std::filesystem::path methylome_dir;
  std::filesystem::path index_file_dir;
  std::unique_ptr<methylome_set> mock_methylome_set;
  std::unique_ptr<genome_index_set> mock_genome_index_set;
  std::unique_ptr<request_handler> mock_request_handler;
};

TEST_F(request_handler_mock, add_response_size_for_bins_success) {
  static constexpr auto index_hash = 0;
  static constexpr auto bin_size = 100;
  // (below) comes from eFlareon chrom sizes and the given bin_size
  static constexpr auto expected_response_size_n_bins = 37;
  request req{"eFlareon_brain", request_type_code::bins, index_hash, bin_size};
  response_header resp_hdr;

  mock_request_handler->add_response_size(req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_error_code::ok);
  EXPECT_EQ(resp_hdr.response_size, expected_response_size_n_bins);
}

TEST_F(request_handler_mock, add_response_size_for_bins_methylome_error) {
  static constexpr auto non_existent_methylome_accession = "eFlareon_brainZZZ";
  static constexpr auto index_hash = 0;
  static constexpr auto rq_type = request_type_code::bins;
  static constexpr auto bin_size = 100;
  request req{non_existent_methylome_accession, rq_type, index_hash, bin_size};
  response_header resp_hdr;
  mock_request_handler->add_response_size(req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_error_code::methylome_not_found);
}

TEST_F(request_handler_mock, add_response_size_for_bins_bad_assembly) {
  static constexpr auto real_accession = "eFlareon_brain";
  static constexpr auto fake_accession = "eFlareon_brainZZZ";
  static constexpr auto index_hash = 0;
  static constexpr auto rq_type = request_type_code::bins;
  static constexpr auto bin_size = 100;

  const auto get_mm_json_path = [&](const auto &acc) {
    return methylome_dir / std::format("{}.m16.json", acc);
  };

  std::error_code ec;
  auto meta = methylome_metadata::read(get_mm_json_path(real_accession), ec);
  EXPECT_FALSE(ec);

  meta.assembly = "eUmbreon";
  const auto fake_meta_file = get_mm_json_path(fake_accession);
  ec = meta.write(fake_meta_file);
  EXPECT_FALSE(ec);

  const std::string real_methylome_file =
    methylome_data::compose_filename(methylome_dir, real_accession);
  const std::string fake_methylome_file =
    methylome_data::compose_filename(methylome_dir, fake_accession);

  std::filesystem::copy(real_methylome_file, fake_methylome_file, ec);
  EXPECT_FALSE(ec);

  request req{fake_accession, rq_type, index_hash, bin_size};
  response_header resp_hdr;
  mock_request_handler->add_response_size(req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_error_code::index_not_found);

  if (std::filesystem::exists(fake_methylome_file))
    std::filesystem::remove(fake_methylome_file);
  if (std::filesystem::exists(fake_meta_file))
    std::filesystem::remove(fake_meta_file);
}

TEST_F(request_handler_mock, add_response_size_for_intervals_success) {
  static constexpr auto rq_type = request_type_code::intervals;
  static constexpr auto index_hash = 0;
  static constexpr auto n_intervals = 100;
  static constexpr auto expected_response_size_n_intervals = n_intervals;
  [[maybe_unused]] const auto offsets =
    std::vector<transferase::query_element>(n_intervals);

  const request req{"eFlareon_brain", rq_type, index_hash, n_intervals};
  response_header resp_hdr;
  mock_request_handler->add_response_size(req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_error_code::ok);
  EXPECT_EQ(resp_hdr.response_size, expected_response_size_n_intervals);
}

TEST_F(request_handler_mock, handle_request_success) {
  static constexpr auto rq_type = request_type_code::intervals;
  static constexpr auto n_intervals = 100;
  static constexpr auto index_hash = 0;
  static constexpr auto expected_response_size_n_intervals = n_intervals;

  [[maybe_unused]] const auto offsets =
    std::vector<transferase::query_element>(n_intervals);
  const request req{"eFlareon_brain", rq_type, index_hash, n_intervals};
  response_header resp_hdr;

  mock_request_handler->add_response_size(req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_error_code::ok);
  EXPECT_EQ(resp_hdr.response_size, expected_response_size_n_intervals);

  mock_request_handler->handle_request(req, resp_hdr);

  EXPECT_EQ(std::size(mock_request_handler->methylomes.accession_to_methylome),
            1);
}

TEST_F(request_handler_mock, handle_request_bad_state) {
  static constexpr auto index_hash = 0;
  static constexpr auto ok_accession = "eFlareon_brain";
  static constexpr auto malformed_accession = "eFlareon_..brain";
  static constexpr auto valid_rq_type = request_type_code::intervals;
  // ADS: (below) not a valid type
  static constexpr auto invalid_rq_type = static_cast<request_type_code>(5);
  static constexpr auto n_intervals = 100;

  request req{malformed_accession, valid_rq_type, index_hash, n_intervals};

  response_header resp_hdr;
  mock_request_handler->handle_request(req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_error_code::invalid_accession);

  req = request{ok_accession, invalid_rq_type, index_hash, n_intervals};
  mock_request_handler->handle_request(req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_error_code::invalid_request_type);
}

TEST_F(request_handler_mock, handle_request_failure) {
  static constexpr auto index_hash = 0;
  static constexpr auto non_existent_accession{"eFlareon_brainZZZ"};
  static constexpr auto rq_type = request_type_code::intervals;
  static constexpr auto n_intervals = 100;
  static constexpr auto expected_response_size_n_intervals = n_intervals;
  [[maybe_unused]] const auto offsets =
    std::vector<transferase::query_element>(n_intervals);

  request req{"eFlareon_brain", rq_type, index_hash, n_intervals};

  response_header resp_hdr;
  mock_request_handler->add_response_size(req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_error_code::ok);
  EXPECT_EQ(resp_hdr.response_size, expected_response_size_n_intervals);
  EXPECT_EQ(std::size(mock_request_handler->methylomes.accession_to_methylome),
            1);

  req.accession = non_existent_accession;
  mock_request_handler->handle_request(req, resp_hdr);

  EXPECT_EQ(std::size(mock_request_handler->methylomes.accession_to_methylome),
            1);
  EXPECT_EQ(resp_hdr.status, server_error_code::methylome_not_found);
}

TEST_F(request_handler_mock, handle_get_levels_intervals_success) {
  static constexpr auto index_hash = 0;
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

  request req{methylome_name, rq_type, index_hash, std::size(intervals)};
  response_header resp_hdr;

  mock_request_handler->add_response_size(req, resp_hdr);
  mock_request_handler->handle_request(req, resp_hdr);

  // ADS: payload stays on server side
  response_payload resp_data;
  mock_request_handler->handle_get_levels(req, query, resp_hdr, resp_data);

  const auto req_offset_elem_size = sizeof(transferase::query_element);
  const auto expected_payload_size = req_offset_elem_size * size(query);
  EXPECT_EQ(std::size(resp_data.payload), expected_payload_size);
}

TEST_F(request_handler_mock, handle_get_levels_bins_success) {
  static constexpr auto index_hash = 0;
  static constexpr auto bin_size = 100;
  static constexpr auto rq_type = request_type_code::bins;
  static constexpr auto assembly = "eFlareon";
  static constexpr auto tissue = "brain";
  const auto methylome_name = std::format("{}_{}", assembly, tissue);
  const auto index_path = index_file_dir / std::format("{}.cpg_idx", assembly);

  EXPECT_TRUE(std::filesystem::exists(index_path));

  std::error_code ec;
  const auto index = genome_index::read(index_file_dir, assembly, ec);
  EXPECT_FALSE(ec);

  request req{methylome_name, rq_type, index_hash, bin_size};
  response_header resp_hdr;
  mock_request_handler->add_response_size(req, resp_hdr);
  mock_request_handler->handle_request(req, resp_hdr);

  response_payload resp_data;
  mock_request_handler->handle_get_levels(req, resp_hdr, resp_data);

  const auto expected_n_bins = index.get_n_bins(bin_size);

  const auto req_offset_elem_size = sizeof(transferase::query_element);
  const auto expected_payload_size = req_offset_elem_size * expected_n_bins;
  EXPECT_EQ(std::size(resp_data.payload), expected_payload_size);
}
