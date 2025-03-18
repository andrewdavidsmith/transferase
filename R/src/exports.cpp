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
// Rcpp::RNGScope uses RAII to protect RNG

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

using xfr_client_remote = transferase::methylome_client_remote;
using query_container = transferase::query_container;

RcppExport auto
_transferase_config_xfr(const SEXP genomes, const SEXP config_dir) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  config_xfr(Rcpp::as<std::vector<std::string>>(genomes),
             Rcpp::as<std::string>(config_dir));
  return R_NilValue;
  END_RCPP
}

RcppExport auto
_transferase_set_xfr_log_level(const SEXP log_level) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  set_xfr_log_level(Rcpp::as<std::string>(log_level));
  return R_NilValue;
  END_RCPP
}

RcppExport auto
_transferase_get_xfr_log_level() -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  return Rcpp::wrap(get_xfr_log_level());
  END_RCPP
}

RcppExport auto
_transferase_create_mclient(const SEXP config_dir) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  return Rcpp::wrap(create_mclient(Rcpp::as<std::string>(config_dir)));
  END_RCPP
}

RcppExport auto
_transferase_format_query(const SEXP client, const SEXP genome,
                          const SEXP intervals) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  // clang-format off
  return Rcpp::wrap(format_query(
    Rcpp::as<Rcpp::XPtr<xfr_client_remote>>(client),
    Rcpp::as<std::string>(genome),
    Rcpp::as<Rcpp::DataFrame>(intervals)
  ));
  // clang-format on
  END_RCPP
}

RcppExport auto
_transferase_init_logger() -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  init_logger();
  return R_NilValue;
  END_RCPP
}

// ADS: defintions for exports of query functions below

RcppExport auto
_transferase_query_bins(const SEXP client, const SEXP methylomes,
                        const SEXP bin_size) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  // clang-format off
  return Rcpp::wrap(query_bins(
    Rcpp::as<Rcpp::XPtr<xfr_client_remote>>(client),
    Rcpp::as<std::vector<std::string>>(methylomes),
    Rcpp::as<std::size_t>(bin_size)
  ));
  // clang-format on
  END_RCPP
}

RcppExport auto
_transferase_query_preprocessed(const SEXP client, const SEXP methylomes,
                                const SEXP query) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  // clang-format off
  return Rcpp::wrap(query_preprocessed(
    Rcpp::as<Rcpp::XPtr<xfr_client_remote>>(client),
    Rcpp::as<std::vector<std::string>>(methylomes),
    Rcpp::as<Rcpp::XPtr<query_container>>(query)
  ));
  // clang-format on
  END_RCPP
}

RcppExport auto
_transferase_query_intervals(const SEXP client, const SEXP methylomes,
                             const SEXP genome, const SEXP intervals) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  // clang-format off
  return Rcpp::wrap(query_intervals(
    Rcpp::as<Rcpp::XPtr<xfr_client_remote>>(client),
    Rcpp::as<std::vector<std::string>>(methylomes),
    Rcpp::as<std::string>(genome),
    Rcpp::as<Rcpp::DataFrame>(intervals)
  ));
  // clang-format on
  END_RCPP
}

RcppExport auto
_transferase_query_bins_cov(const SEXP client, const SEXP methylomes,
                            const SEXP bin_size) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  // clang-format off
  return Rcpp::wrap(query_bins_cov(
    Rcpp::as<Rcpp::XPtr<xfr_client_remote>>(client),
    Rcpp::as<std::vector<std::string>>(methylomes),
    Rcpp::as<std::size_t>(bin_size)
  ));
  // clang-format on
  END_RCPP
}

RcppExport auto
_transferase_query_preprocessed_cov(const SEXP client, const SEXP methylomes,
                                    const SEXP query) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  // clang-format off
  return Rcpp::wrap(query_preprocessed_cov(
    Rcpp::as<Rcpp::XPtr<xfr_client_remote>>(client),
    Rcpp::as<std::vector<std::string>>(methylomes),
    Rcpp::as<Rcpp::XPtr<query_container>>(query)
  ));
  // clang-format on
  END_RCPP
}

RcppExport auto
_transferase_query_intervals_cov(const SEXP client, const SEXP methylomes,
                                 const SEXP genome,
                                 const SEXP intervals) -> SEXP {
  BEGIN_RCPP
  Rcpp::RNGScope rcpp_rngScope_gen;
  // clang-format off
  return Rcpp::wrap(query_intervals_cov(
    Rcpp::as<Rcpp::XPtr<xfr_client_remote>>(client),
    Rcpp::as<std::vector<std::string>>(methylomes),
    Rcpp::as<std::string>(genome),
    Rcpp::as<Rcpp::DataFrame>(intervals)
  ));
  // clang-format on
  END_RCPP
}

RcppExport auto
R_init_transferase(DllInfo *dll) {
  // clang-format off
  static const R_CallMethodDef call_entries[] = {
    {"_transferase_config_xfr", (DL_FUNC)&_transferase_config_xfr, 2},
    {"_transferase_set_xfr_log_level", (DL_FUNC)&_transferase_set_xfr_log_level, 1},
    {"_transferase_get_xfr_log_level", (DL_FUNC)&_transferase_get_xfr_log_level, 0},
    {"_transferase_create_mclient", (DL_FUNC)&_transferase_create_mclient, 1},
    {"_transferase_format_query", (DL_FUNC)&_transferase_format_query, 3},
    {"_transferase_init_logger", (DL_FUNC)&_transferase_init_logger, 0},
    {"_transferase_query_bins", (DL_FUNC)&_transferase_query_bins, 3},
    {"_transferase_query_preprocessed", (DL_FUNC)&_transferase_query_preprocessed, 3},
    {"_transferase_query_intervals", (DL_FUNC)&_transferase_query_intervals, 4},
    {"_transferase_query_bins_cov", (DL_FUNC)&_transferase_query_bins_cov, 3},
    {"_transferase_query_preprocessed_cov", (DL_FUNC)&_transferase_query_preprocessed_cov, 3},
    {"_transferase_query_intervals_cov", (DL_FUNC)&_transferase_query_intervals_cov, 4},
    {nullptr, nullptr, 0},
  };
  // clang-format on
  R_registerRoutines(dll, nullptr, call_entries, nullptr, nullptr);
  R_useDynamicSymbols(dll, FALSE);
}
