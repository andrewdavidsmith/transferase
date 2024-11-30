#include <cpg_index_meta.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

// Demonstrate some basic assertions.
TEST(CpgIndexMetaTest, BasicAssertions) {
  cpg_index_meta cim;
  EXPECT_EQ(cim.get_n_cpgs_chrom(), std::vector<std::uint32_t>());
  cim.chrom_offset = {0, 1000, 10000};
  cim.n_cpgs = 11000;
  EXPECT_EQ(cim.get_n_cpgs_chrom(),
            std::vector<std::uint32_t>({1000, 9000, 1000}));
  cim.chrom_offset = {0};
  cim.n_cpgs = 0;
  EXPECT_EQ(cim.get_n_cpgs_chrom(), std::vector<std::uint32_t>({0}));
}
