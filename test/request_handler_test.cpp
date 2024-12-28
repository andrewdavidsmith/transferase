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

#include "request_handler.hpp"

#include "cpg_index.hpp"
#include "cpg_index_data.hpp"  // for cpg_index::data::get_offsets
#include "cpg_index_metadata.hpp"
#include "genomic_interval.hpp"
#include "logger.hpp"  // ADS: so we can setup the logger
#include "methylome_metadata.hpp"
#include "request.hpp"
#include "response.hpp"
#include "xfrase_error.hpp"

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

TEST(request_handler_test, basic_assertions) {
  std::error_code ec;
  request_handler rh("data", "data", 8, ec);
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

    logger::instance(shared_from_cout(), "command", xfrase_log_level::critical);

    mock_methylome_set =
      std::make_unique<methylome_set>(methylome_dir, max_live_methylomes);
    std::error_code unused_ec;
    mock_cpg_index_set =
      std::make_unique<cpg_index_set>(index_file_dir, unused_ec);
    mock_request_handler = std::make_unique<request_handler>(
      methylome_dir, index_file_dir, max_live_methylomes, unused_ec);
    std::ignore = unused_ec;
  }

  void
  TearDown() override {}

  std::uint32_t max_live_methylomes{};
  std::filesystem::path raw_data_dir;
  std::filesystem::path methylome_dir;
  std::filesystem::path index_file_dir;
  std::unique_ptr<methylome_set> mock_methylome_set;
  std::unique_ptr<cpg_index_set> mock_cpg_index_set;
  std::unique_ptr<request_handler> mock_request_handler;
};

TEST_F(request_handler_mock, add_response_size_for_bins_success) {
  static constexpr auto eFlareon_methylome_size = 234;
  static constexpr auto rq_type = request_header::request_type::bin_counts;
  static constexpr auto bin_size = 100;
  // (below) comes from eFlareon chrom sizes and the given bin_size
  static constexpr auto expected_response_size_n_bins = 37;
  request_header req_hdr{"eFlareon_brain", eFlareon_methylome_size, rq_type};
  bins_request req{bin_size};
  response_header resp_hdr;

  mock_request_handler->add_response_size_for_bins(req_hdr, req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_response_code::ok);
  EXPECT_EQ(resp_hdr.response_size, expected_response_size_n_bins);
}

TEST_F(request_handler_mock, add_response_size_for_bins_methylome_error) {
  static constexpr auto non_existent_methylome_accession = "eFlareon_brainZZZ";
  static constexpr auto eFlareon_methylome_size = 234;
  static constexpr auto rq_type = request_header::request_type::bin_counts;
  static constexpr auto bin_size = 100;
  request_header req_hdr{non_existent_methylome_accession,
                         eFlareon_methylome_size, rq_type};
  bins_request req{bin_size};
  response_header resp_hdr;
  mock_request_handler->add_response_size_for_bins(req_hdr, req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_response_code::methylome_not_found);
}

TEST_F(request_handler_mock, add_response_size_for_bins_bad_assembly) {
  static constexpr auto real_accession = "eFlareon_brain";
  static constexpr auto fake_accession = "eFlareon_brainZZZ";
  static constexpr auto eFlareon_methylome_size = 234;
  static constexpr auto rq_type = request_header::request_type::bin_counts;
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

  const std::filesystem::path real_methylome_file =
    methylome_dir / std::format("{}.m16", real_accession);
  const std::filesystem::path fake_methylome_file =
    methylome_dir / std::format("{}.m16", fake_accession);
  std::filesystem::copy(real_methylome_file, fake_methylome_file, ec);
  EXPECT_FALSE(ec);

  request_header req_hdr{fake_accession, eFlareon_methylome_size, rq_type};
  bins_request req{bin_size};
  response_header resp_hdr;
  mock_request_handler->add_response_size_for_bins(req_hdr, req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_response_code::index_not_found);

  if (std::filesystem::exists(fake_methylome_file))
    std::filesystem::remove(fake_methylome_file);
  if (std::filesystem::exists(fake_meta_file))
    std::filesystem::remove(fake_meta_file);
}

TEST_F(request_handler_mock, add_response_size_for_intervals_success) {
  static constexpr auto eFlareon_methylome_size = 234;
  static constexpr auto rq_type = request_header::request_type::counts;
  static constexpr auto n_intervals = 100;
  static constexpr auto expected_response_size_n_intervals = n_intervals;
  const auto offsets = std::vector<request::offset_type>(n_intervals);
  const request req{n_intervals, offsets};
  request_header req_hdr{"eFlareon_brain", eFlareon_methylome_size, rq_type};
  response_header resp_hdr;

  mock_request_handler->add_response_size_for_intervals(req_hdr, req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_response_code::ok);
  EXPECT_EQ(resp_hdr.response_size, expected_response_size_n_intervals);
}

TEST_F(request_handler_mock, handle_header_success) {
  static constexpr auto eFlareon_methylome_size = 234;
  static constexpr auto rq_type = request_header::request_type::counts;
  static constexpr auto n_intervals = 100;
  static constexpr auto expected_response_size_n_intervals = n_intervals;
  const auto offsets = std::vector<request::offset_type>(n_intervals);
  const request req{n_intervals, offsets};
  request_header req_hdr{"eFlareon_brain", eFlareon_methylome_size, rq_type};
  response_header resp_hdr;

  mock_request_handler->add_response_size_for_intervals(req_hdr, req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_response_code::ok);
  EXPECT_EQ(resp_hdr.response_size, expected_response_size_n_intervals);

  mock_request_handler->handle_header(req_hdr, resp_hdr);

  EXPECT_EQ(std::size(mock_request_handler->ms.accession_to_methylome), 1);
}

TEST_F(request_handler_mock, handle_header_bad_state) {
  static constexpr auto ok_accession = "eFlareon_brain";
  static constexpr auto malformed_accession = "eFlareon_..brain";
  static constexpr auto valid_rq_type = request_header::request_type::counts;
  // ADS: (below) not a valid type
  static constexpr auto invalid_rq_type =
    static_cast<request_header::request_type>(1000);
  static constexpr auto eFlareon_methylome_size = 234;

  request_header req_hdr{malformed_accession, eFlareon_methylome_size,
                         valid_rq_type};
  response_header resp_hdr;
  mock_request_handler->handle_header(req_hdr, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_response_code::invalid_accession);

  req_hdr =
    request_header{ok_accession, eFlareon_methylome_size, invalid_rq_type};
  mock_request_handler->handle_header(req_hdr, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_response_code::invalid_request_type);
}

TEST_F(request_handler_mock, handle_header_failure) {
  static constexpr auto eFlareon_methylome_size = 234;
  static constexpr auto non_existent_accession{"eFlareon_brainZZZ"};
  static constexpr auto rq_type = request_header::request_type::counts;
  static constexpr auto n_intervals = 100;
  static constexpr auto expected_response_size_n_intervals = n_intervals;
  const auto offsets = std::vector<request::offset_type>(n_intervals);
  const request req{n_intervals, offsets};
  request_header req_hdr{"eFlareon_brain", eFlareon_methylome_size, rq_type};
  response_header resp_hdr;

  mock_request_handler->add_response_size_for_intervals(req_hdr, req, resp_hdr);

  EXPECT_EQ(resp_hdr.status, server_response_code::ok);
  EXPECT_EQ(resp_hdr.response_size, expected_response_size_n_intervals);

  req_hdr.accession = non_existent_accession;
  mock_request_handler->handle_header(req_hdr, resp_hdr);

  EXPECT_EQ(std::size(mock_request_handler->ms.accession_to_methylome), 0);
  EXPECT_EQ(resp_hdr.status, server_response_code::methylome_not_found);
}

TEST_F(request_handler_mock, handle_get_counts_success) {
  static constexpr auto rq_type = request_header::request_type::counts;
  static constexpr auto assembly = "eFlareon";
  static constexpr auto tissue = "brain";
  const auto methylome_name = std::format("{}_{}", assembly, tissue);
  const auto intervals_file = std::format("{}_hmr.bed", methylome_name);
  const auto intervals_path = raw_data_dir / intervals_file;
  const auto index_path = index_file_dir / std::format("{}.cpg_idx", assembly);

  EXPECT_TRUE(std::filesystem::exists(intervals_path));
  EXPECT_TRUE(std::filesystem::exists(index_path));

  std::error_code ec;
  const auto index = cpg_index::read(index_file_dir, assembly, ec);
  EXPECT_FALSE(ec);
  const auto [gis, gis_ec] = genomic_interval::load(index.meta, intervals_path);
  EXPECT_FALSE(gis_ec);

  const auto offsets = index.data.get_query(index.meta, gis);

  request req{static_cast<std::uint32_t>(std::size(gis)), offsets};
  request_header req_hdr{methylome_name, index.meta.n_cpgs, rq_type};
  response_header resp_hdr;

  mock_request_handler->add_response_size_for_intervals(req_hdr, req, resp_hdr);
  mock_request_handler->handle_header(req_hdr, resp_hdr);

  // ADS: payload stays on server side
  response_payload resp_data;
  mock_request_handler->handle_get_counts(req_hdr, req, resp_hdr, resp_data);

  const auto req_offset_elem_size = sizeof(request::offset_type);
  const auto expected_payload_size = req_offset_elem_size * std::size(offsets);
  EXPECT_EQ(std::size(resp_data.payload), expected_payload_size);
}

TEST_F(request_handler_mock, handle_get_bins_success) {
  static constexpr auto bin_size = 100;
  static constexpr auto rq_type = request_header::request_type::bin_counts;
  static constexpr auto assembly = "eFlareon";
  static constexpr auto tissue = "brain";
  const auto methylome_name = std::format("{}_{}", assembly, tissue);
  const auto index_path = index_file_dir / std::format("{}.cpg_idx", assembly);

  EXPECT_TRUE(std::filesystem::exists(index_path));

  std::error_code ec;
  const auto index = cpg_index::read(index_file_dir, assembly, ec);
  EXPECT_FALSE(ec);

  bins_request req{bin_size};
  request_header req_hdr{methylome_name, index.meta.n_cpgs, rq_type};
  response_header resp_hdr;

  mock_request_handler->add_response_size_for_bins(req_hdr, req, resp_hdr);
  mock_request_handler->handle_header(req_hdr, resp_hdr);

  response_payload resp_data;
  mock_request_handler->handle_get_bins(req_hdr, req, resp_hdr, resp_data);

  const auto expected_n_bins = index.meta.get_n_bins(bin_size);

  const auto req_offset_elem_size = sizeof(request::offset_type);
  const auto expected_payload_size = req_offset_elem_size * expected_n_bins;
  EXPECT_EQ(std::size(resp_data.payload), expected_payload_size);
}
