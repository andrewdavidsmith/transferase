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

// ADS: This file started out as generated from Rcpp (RcppExports.cpp), but I
// had to modify it starting with inclusion of headers, and because it only
// understands the simplest scenarios of trailing return types. I.e. the
// forward declarations were failing. I assume I will need a file to fill the
// role of this one, but I expect it will move away from Rcpp when I get the
// time to explore other options.
//
// RCPP_BEGIN/END wrap code in a try/catch
// RcppExport sets C linkage and default visibility

#include "transferase_r.hpp"

#include "methylome_client_remote.hpp"
#include "query_container.hpp"

#include <Rcpp.h>

#include <cstddef>
#include <string>
#include <vector>

// ADS: left as a choice for installer or binary package maintainer...
#ifdef RCPP_USE_GLOBAL_ROSTREAM
Rcpp::Rostream<true> &Rcpp::Rcout = Rcpp::Rcpp_cout_get();
Rcpp::Rostream<false> &Rcpp::Rcerr = Rcpp::Rcpp_cerr_get();
#endif

template <typename T> using rcpp_param = Rcpp::traits::input_parameter<T>;
using xfr_client_remote = transferase::methylome_client_remote;
using query_container = transferase::query_container;

auto
configXfr(const std::vector<std::string> &genomes,
          const std::string &config_dir) -> void;

auto
setXfrLogLevel(const std::string &log_level) -> void;

auto
initLogger() -> void;

[[nodiscard]] auto
getXfrLogLevel() -> std::string;

[[nodiscard]] auto
createMClient(const std::string &config_dir) -> Rcpp::XPtr<xfr_client_remote>;

auto
formatQuery(const Rcpp::XPtr<xfr_client_remote> client,
            const std::string &genome,
            const Rcpp::DataFrame intervals) -> Rcpp::XPtr<query_container>;

RcppExport auto
_transferase_configXfr(const SEXP genomesSEXP,
                       const SEXP config_dirSEXP) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  rcpp_param<const std::vector<std::string> &>::type genomes(genomesSEXP);
  rcpp_param<const std::string &>::type config_dir(config_dirSEXP);
  configXfr(genomes, config_dir);
  return R_NilValue;
  END_RCPP
}

RcppExport auto
_transferase_setXfrLogLevel(const SEXP log_levelSEXP) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  rcpp_param<const std::string &>::type log_level(log_levelSEXP);
  setXfrLogLevel(log_level);
  return R_NilValue;
  END_RCPP
}

RcppExport auto
_transferase_getXfrLogLevel() -> SEXP {
  BEGIN_RCPP
  Rcpp::RObject rcpp_result_gen;
  Rcpp::RNGScope rcpp_rngScope_gen;
  rcpp_result_gen = Rcpp::wrap(getXfrLogLevel());
  return rcpp_result_gen;
  END_RCPP
}

RcppExport auto
_transferase_createMClient(const SEXP config_dirSEXP) -> SEXP {
  BEGIN_RCPP
  Rcpp::RObject rcpp_result_gen;
  Rcpp::RNGScope rcpp_rngScope_gen;
  rcpp_param<const std::string &>::type config_dir(config_dirSEXP);
  rcpp_result_gen = Rcpp::wrap(createMClient(config_dir));
  return rcpp_result_gen;
  END_RCPP
}

RcppExport auto
_transferase_formatQuery(const SEXP clientSEXP, const SEXP genomeSEXP,
                         const SEXP intervalsSEXP) -> SEXP {
  BEGIN_RCPP
  Rcpp::RObject rcpp_result_gen;
  Rcpp::RNGScope rcpp_rngScope_gen;
  rcpp_param<const Rcpp::XPtr<xfr_client_remote>>::type client(clientSEXP);
  rcpp_param<const std::string &>::type genome(genomeSEXP);
  rcpp_param<const Rcpp::DataFrame>::type intervals(intervalsSEXP);
  rcpp_result_gen = Rcpp::wrap(formatQuery(client, genome, intervals));
  return rcpp_result_gen;
  END_RCPP
}

RcppExport auto
_transferase_initLogger() -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  initLogger();
  return R_NilValue;
  END_RCPP
}

// ADS: defintions for exports of query functions below

RcppExport auto
_transferase_queryBins(SEXP clientSEXP, SEXP methylomesSEXP,
                       SEXP bin_sizeSEXP) -> SEXP {
  BEGIN_RCPP
  Rcpp::RObject rcpp_result_gen;
  Rcpp::RNGScope rcpp_rngScope_gen;
  rcpp_param<const Rcpp::XPtr<xfr_client_remote>>::type client(clientSEXP);
  rcpp_param<const std::vector<std::string> &>::type methylomes(methylomesSEXP);
  rcpp_param<const std::size_t>::type bin_size(bin_sizeSEXP);
  rcpp_result_gen = Rcpp::wrap(queryBins(client, methylomes, bin_size));
  return rcpp_result_gen;
  END_RCPP
}

RcppExport auto
_transferase_queryPreprocessed(const SEXP clientSEXP, const SEXP methylomesSEXP,
                               const SEXP querySEXP) -> SEXP {
  BEGIN_RCPP
  Rcpp::RObject rcpp_result_gen;
  Rcpp::RNGScope rcpp_rngScope_gen;
  rcpp_param<const Rcpp::XPtr<xfr_client_remote>>::type client(clientSEXP);
  rcpp_param<const std::vector<std::string> &>::type methylomes(methylomesSEXP);
  rcpp_param<const Rcpp::XPtr<query_container>>::type query(querySEXP);
  rcpp_result_gen = Rcpp::wrap(queryPreprocessed(client, methylomes, query));
  return rcpp_result_gen;
  END_RCPP
}

RcppExport auto
_transferase_queryIntervals(const SEXP clientSEXP, const SEXP methylomesSEXP,
                            const SEXP genomeSEXP,
                            const SEXP intervalsSEXP) -> SEXP {
  BEGIN_RCPP
  Rcpp::RObject rcpp_result_gen;
  Rcpp::RNGScope rcpp_rngScope_gen;
  rcpp_param<const Rcpp::XPtr<xfr_client_remote>>::type client(clientSEXP);
  rcpp_param<const std::vector<std::string> &>::type methylomes(methylomesSEXP);
  rcpp_param<const std::string &>::type genome(genomeSEXP);
  rcpp_param<const Rcpp::DataFrame>::type intervals(intervalsSEXP);
  rcpp_result_gen =
    Rcpp::wrap(queryIntervals(client, methylomes, genome, intervals));
  return rcpp_result_gen;
  END_RCPP
}

RcppExport auto
_transferase_queryBinsCov(const SEXP clientSEXP, const SEXP methylomesSEXP,
                          const SEXP bin_sizeSEXP) -> SEXP {
  BEGIN_RCPP
  Rcpp::RObject rcpp_result_gen;
  Rcpp::RNGScope rcpp_rngScope_gen;
  rcpp_param<const Rcpp::XPtr<xfr_client_remote>>::type client(clientSEXP);
  rcpp_param<const std::vector<std::string> &>::type methylomes(methylomesSEXP);
  rcpp_param<const std::size_t>::type bin_size(bin_sizeSEXP);
  rcpp_result_gen = Rcpp::wrap(queryBinsCov(client, methylomes, bin_size));
  return rcpp_result_gen;
  END_RCPP
}

RcppExport auto
_transferase_queryPreprocessedCov(const SEXP clientSEXP,
                                  const SEXP methylomesSEXP,
                                  const SEXP querySEXP) -> SEXP {
  BEGIN_RCPP
  Rcpp::RObject rcpp_result_gen;
  Rcpp::RNGScope rcpp_rngScope_gen;
  rcpp_param<const Rcpp::XPtr<xfr_client_remote>>::type client(clientSEXP);
  rcpp_param<const std::vector<std::string> &>::type methylomes(methylomesSEXP);
  rcpp_param<const Rcpp::XPtr<query_container>>::type query(querySEXP);
  rcpp_result_gen = Rcpp::wrap(queryPreprocessedCov(client, methylomes, query));
  return rcpp_result_gen;
  END_RCPP
}

RcppExport auto
_transferase_queryIntervalsCov(const SEXP clientSEXP, const SEXP methylomesSEXP,
                               const SEXP genomeSEXP,
                               const SEXP intervalsSEXP) -> SEXP {
  BEGIN_RCPP
  Rcpp::RObject rcpp_result_gen;
  Rcpp::RNGScope rcpp_rngScope_gen;
  rcpp_param<const Rcpp::XPtr<xfr_client_remote>>::type client(clientSEXP);
  rcpp_param<const std::vector<std::string> &>::type methylomes(methylomesSEXP);
  rcpp_param<const std::string &>::type genome(genomeSEXP);
  rcpp_param<const Rcpp::DataFrame>::type intervals(intervalsSEXP);
  rcpp_result_gen =
    Rcpp::wrap(queryIntervalsCov(client, methylomes, genome, intervals));
  return rcpp_result_gen;
  END_RCPP
}

RcppExport auto
R_init_transferase(DllInfo *dll) -> void {
  // clang-format off
  static const R_CallMethodDef CallEntries[] = {
    {"_transferase_configXfr", (DL_FUNC)&_transferase_configXfr, 2},
    {"_transferase_setXfrLogLevel", (DL_FUNC)&_transferase_setXfrLogLevel, 1},
    {"_transferase_getXfrLogLevel", (DL_FUNC)&_transferase_getXfrLogLevel, 0},
    {"_transferase_createMClient", (DL_FUNC)&_transferase_createMClient, 1},
    {"_transferase_formatQuery", (DL_FUNC)&_transferase_formatQuery, 3},
    {"_transferase_initLogger", (DL_FUNC)&_transferase_initLogger, 0},
    {"_transferase_queryBins", (DL_FUNC)&_transferase_queryBins, 3},
    {"_transferase_queryPreprocessed", (DL_FUNC)&_transferase_queryPreprocessed, 3},
    {"_transferase_queryIntervals", (DL_FUNC)&_transferase_queryIntervals, 4},
    {"_transferase_queryBinsCov", (DL_FUNC)&_transferase_queryBinsCov, 3},
    {"_transferase_queryPreprocessedCov", (DL_FUNC)&_transferase_queryPreprocessedCov, 3},
    {"_transferase_queryIntervalsCov", (DL_FUNC)&_transferase_queryIntervalsCov, 4},
    {nullptr, nullptr, 0},
  };
  // clang-format on
  R_registerRoutines(dll, nullptr, CallEntries, nullptr, nullptr);
  R_useDynamicSymbols(dll, FALSE);
}
