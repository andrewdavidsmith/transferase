# MIT License
#
# Copyright (c) 2025 Andrew Smith
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

library(R6)

#' Configure transferase
#'
#' The config_xfr function configures transferase, which includes creating a
#' configuration directory, writing configuration files, and typically
#' downloading metadata and configuration data corresponding to one or more
#' specified reference genomes.
#'
#' @param genomes A list of names of reference genome assemblies. These are
#' in the format used by the UCSC Genome Browser (i.e., "hg38" instead of
#' "GRCh38"). There is no way to know if a typo has been made, as the system
#' will simply assume an incorrect genome name is one that is not supported.
#' @param config_dir A directory to place the configuration. Default is
#' `$HOME/.config/transferase`.
#'
#' @export
#' @examples
#' # Mostly leave the config_dir empty
#' config_xfr(c("hg38"), "usually_empty_but_in_examples")
config_xfr <- function(genomes, config_dir = "") {
  invisible(.Call(`_Rxfr_config_xfr`, genomes, config_dir))
}

#' Set transferase log level
#'
#' The `set_xfr_log_level` function sets the log level for functions related
#' to transferase. Must be one of 'debug', 'info', 'warning', 'error' or
#' 'critical'. The value 'debug' prints the most information, while 'critical'
#' could print nothing even if there is an error.
#'
#' @param log_level The log level value to use. One of debug, info, warning,
#' error or critical.
#'
#' @export
#' @examples
#' set_xfr_log_level("debug")
set_xfr_log_level <- function(log_level) {
  invisible(.Call(`_Rxfr_set_xfr_log_level`, log_level))
}

#' Get the transferase log level
#'
#' Get the current log level, which is the default if you didn't use
#' `set_xfr_log_level`, or whatever log level you most recently set it to.
#'
#' @return A string indicating the current log level
#'
#' @export
#' @examples
#' get_xfr_log_level()
get_xfr_log_level <- function() {
  .Call(`_Rxfr_get_xfr_log_level`)
}

#' Load transferase metadata
#'
#' Load a data frame with the current transferase metadata.
#'
#' @param genome The reference genome for which you want metadata.
#'
#' @param config_dir The configuration directory. Unless set specifically with
#'   the config_xfr() function, this should be left empty.
#'
#' @return A data frame with the current transferase metadata.
#'
#' @details
#'
#' The following information is available for each methylome:
#'
#' study: The SRA study accession for the corresponding study.
#'
#' experiment: The SRA experiment accession, and the identifier for the
#' methylome.
#'
#' assembly: This is the genome. I might rename this to 'genome' soon.
#'
#' bsrate: Estimated bisulfite conversion rate.
#'
#' meth: Estimated global weighted mean methylation level.
#'
#' depth: The average number of reads covering each CpG site.
#'
#' mapping: The fraction of *uniquely* mapping reads. Can be far less than the
#' fraction that map.
#'
#' uniq: The fraction of uniquely mapping reads that are not duplicates.
#'
#' sites: The fraction of CpG sites covered by at least one read.
#'
#' pmd_count: Number of PMDs identified in the methylome. Note: this number
#' will be low, but rarely zero, for methylomes that do not exhibit PMDs at
#' all.  If the fraction of the genome covered by PMDs (pmd_count x pmd_size)
#' is less than 10 percent, any identified PMDs are sure false-positives.
#'
#' pmd_size: The mean size of identified PMDs.
#'
#' hmr_count`: Number of identified HMRs. If this number is much more than
#' 100K, then many are likely false-positives.
#'
#' hmr_size: The mean size of identified HMRs. If these are too large, then
#' they could easily include some very deep but short PMDs.
#'
#' sample_name: A label currently associated with the methylome. These are
#' inferred based on annotations in SRA and GEO, and depend heavily on how the
#' original data submitter chose to describe their data. There have been cases
#' where these are wrong because, for example, all samples in the same study
#' were given a name related to the study/project, rather than the individual
#' sample, and only detailed reading can uncover the differences. Your
#' feedback can help improve these.
#'
#' details: General metadata about the sample, in long form.
#'
#' @export
#' @examples
#' # Mostly leave the config_dir empty
#' config_xfr(c("hg38"), "usually_empty_but_in_examples")
#' meta <- load_xfr_metadata("hg38", "usually_empty_but_in_examples")
load_xfr_metadata <- function(genome, config_dir = "") {
  if (config_dir == "") {
    home_dir <- Sys.getenv("HOME")
    config_dir <- file.path(home_dir, ".config", "transferase")
  } else if (!file.exists(config_dir)) {
    fmt <- "directory does not exist: %s. See the config_xfr function"
    stop(sprintf(fmt, config_dir), call. = FALSE)
  }
  metadata_filename <- sprintf("metadata_dataframe_%s.tsv",
                               packageVersion("Rxfr"))
  metadata_path <- file.path(config_dir, metadata_filename)
  if (!file.exists(metadata_path)) {
    fmt <- "config file does not exist: %s. See the config_xfr function"
    stop(sprintf(fmt, metadata_path), call. = FALSE)
  }
  full_table <- read.table(metadata_path, header=TRUE)
  selected <- full_table[, "assembly"] == genome
  full_table[selected, ]
}

#' Initialize logger
#'
#' Initialize logging for transferase. This function is called automatically
#' when 'library(Rxfr)' is called. You will never need to use it.
#'
#' @export
#' @examples
#' init_logger()
init_logger <- function() {
  .Call(`_Rxfr_init_logger`)
}

#' @title Objects to accelerate repeated queries
#'
#' @description
#' Instances of MQuery hold preprocessed genomic intervals for faster queries.
MQuery <- R6Class(
  "MQuery",
  private = list(
    .data = NULL,
    .genome = NULL,
    .size = NA
  ),
  active = list(

    #' @field data This is the internal data of the MQuery object. It cannot be
    #' inspected or manipulated directly.
    data = function(value) {
      if (missing(value)) {
        private$.data
      } else {
        stop("`$data` is read only", call. = FALSE)
      }
    },

    #' @field genome The corresponding reference genome. Do not use an MQuery
    #' object to query methylomes for any other genome.
    genome = function(value) {
      if (missing(value)) {
        private$.genome
      } else {
        stop("`$genome` is read only", call. = FALSE)
      }
    },

    #' @field size The number of intervals represented in the MQuery object.
    size = function(value) {
      if (missing(value)) {
        private$.size
      } else {
        stop("`$size` is read only", call. = FALSE)
      }
    }
  ),
  public = list(

    #' @description Construct an MQuery object
    #'
    #' This constructor is not callable, as one of its instance variables
    #' cannot be created within R. The only way to construct an MQuery object
    #' is with the MClient$format_query() method.
    #'
    #' @param data This is the internal data of the MQuery object. It cannot be
    #' inspected or manipulated directly.
    #'
    #' @param genome The corresponding reference genome. Do not use an MQuery object
    #' to query methylomes for any other genome.
    #'
    #' @param size The number of intervals represented in the MQuery object.
    initialize = function(data, genome, size) {
      private$.data <- data
      private$.genome <- genome
      private$.size <- size
    },

    #' @description Print an MQuery object
    #' @param ... None
    print = function(...) {
      cat("MQuery:\n")
      cat("genome: ", private$.genome, "\n", sep = "")
      cat("size:   ", private$.size, "\n", sep = "")
      cat("data:   ", typeof(private$.data), "\n", sep = "")
    }
  )
)

#' @title An interface for queries
#'
#' @description
#' Instances of MClient are an interface for making queries in transferase.
MClient <- R6Class(
  "MClient",
  private = list(
    client = NULL
  ),
  public = list(

    #' @field config_dir The name of the configuration directory associated with this
    #' MClient object. Again, this would have been configured using `config_xfr`
    #' or the command line transferase app.
    config_dir = NULL,

    #' @description Create an MClient object
    #'
    #' The constructor can take the name of a configuration directory, but most
    #' users will leave this empty and allow it to use the default.
    #'
    #' @param config_dir The name of a configuration directory; most users will
    #' typically leave this empty.
    #'
    #' @export
    #' @examples
    #' # Mostly leave the config_dir empty
    #' config_xfr(c("hg38"), "usually_empty_but_in_examples")
    #' client <- MClient$new("usually_empty_but_in_examples")
    initialize = function(config_dir = "") {
      if (config_dir == "") {
        home_dir <- Sys.getenv("HOME")
        config_dir <- file.path(home_dir, ".config", "transferase")
      } else if (!file.exists(config_dir)) {
        fmt <- "directory does not exist: %s. See the config_xfr function"
        stop(sprintf(fmt, config_dir), call. = FALSE)
      }
      config_filename <- sprintf("transferase_client_%s.json",
                                 packageVersion("Rxfr"))
      config_file_path <- file.path(config_dir, config_filename)
      if (!file.exists(config_file_path)) {
        fmt <- "config file does not exist: %s. See the config_xfr function"
        stop(sprintf(fmt, config_file_path), call. = FALSE)
      }
      self$config_dir <- config_dir
      private$client <- .Call(`_Rxfr_create_mclient`, config_dir)
    },

    #' @description Print an MClient object
    #' @param ... None
    print = function(...) {
      cat("MClient:\n")
      cat("config_dir: ", self$config_dir, "\n", sep = "")
      cat("client:     ", typeof(private$client), "\n", sep = "")
    },

    #' @description
    #' Performs a transferase query
    #'
    #' The do_query function performs all the work of querying the transferase
    #' system for methylation levels. It takes methylome names and query
    #' intervals. It returns methylation levels for each query interval and
    #' methylome, in a matrix with rows corresponding to query intervals and
    #' columns corresponding to methylomes. It allows for query intervals to be
    #' specified in multiple formats and has options that control how the
    #' output is structured.
    #'
    #' @param methylomes A list of methylome names, each of which is a string.
    #'
    #' @param query A specification of the query. See "Details".
    #'
    #' @param genome The reference genome corresponding to the methylomes and
    #' query. If the query is in MQuery format, this argument is not needed.
    #'
    #' @param covered If TRUE, the query will return, for each query interval
    #' and queried methylome, the number of CpG sites that are covered by at
    #' least one read.
    #'
    #' @param add_n_cpgs If TRUE, the total number of CpG sites in each query
    #' interval with be provided as the final column in the matrix returned by
    #' this function.
    #'
    #' @param add_header If true, a header will be added to the matrix
    #' returned by this function.
    #'
    #' @param add_rownames If true, rownames will be added to the matrix
    #' returned by this function.
    #'
    #' @param header_sep The separator character to use inside column names in
    #' the returned matrix.
    #'
    #' @param rowname_sep The separator character to use inside row names in
    #' the returned matrix.
    #'
    #' @export
    #' @examples
    #' # Mostly leave the config_dir empty
    #' config_xfr(c("hg38"), "usually_empty_but_in_examples")
    #' client <- MClient$new("usually_empty_but_in_examples")
    #'
    #' genome <- "hg38"
    #' methylomes <- c("SRX3468816", "SRX3468835")
    #' intervals <- data.frame(chrom=c("chr1", "chr2", "chr3"),
    #'                         start=c(10468, 197437, 308977),
    #'                         stops=c(11240, 198535, 309210))
    #'
    #' levels <- client$do_query(methylomes, intervals, genome)
    do_query = function(methylomes, query,
                        genome = NULL,
                        covered = FALSE,
                        add_n_cpgs = FALSE,
                        add_header = FALSE,
                        add_rownames = FALSE,
                        header_sep = '_',
                        rowname_sep = '_') {

      is_bins <- function(query) {
        is.atomic(query) && length(query) == 1 && is.numeric(query)
      }

      is_mquery <- function(query) {
        class(query)[1] == "MQuery"
      }

      is_intervals <- function(query) {
        is.data.frame(query)
      }

      if (is.null(genome)) {
        if (is_mquery(query)) {
          genome = query$genome
        } else {
          msg <- "'genome' is required unless query is an MQuery object"
          stop(msg, call. = FALSE)
        }
      }

      get_query_response <- function(genome, methylomes, query, covered) {
        if (is_intervals(query)) {
          if (covered) {
            .Call(`_Rxfr_query_intervals_cov`, private$client, genome,
                  methylomes, query)
          } else {
            .Call(`_Rxfr_query_intervals`, private$client, genome,
                  methylomes, query)
          }
        } else if (is_bins(query)) {
          if (covered) {
            .Call(`_Rxfr_query_bins_cov`, private$client, genome,
                  methylomes, query)
          } else {
            .Call(`_Rxfr_query_bins`, private$client, genome,
                  methylomes, query)
          }
        } else if (is_mquery(query)) {
          if (covered) {
            .Call(`_Rxfr_query_preprocessed_cov`, private$client, genome,
                  methylomes, query$data)
          } else {
            .Call(`_Rxfr_query_preprocessed`, private$client, genome,
                  methylomes, query$data)
          }
        } else {
          stop("Invalid 'query' argument", call. = FALSE)
        }
      }
      response <- get_query_response(genome, methylomes, query, covered)

      # Add a header to the results
      if (add_header) {
        sub_headings <- if (covered) c("M", "U", "C") else c("M", "U")
        header <- c(t(outer(methylomes, sub_headings, paste, sep = header_sep)))
        colnames(response) <- header
      }

      # Add rownames to the results
      if (add_rownames) {
        if (is_bins(query)) {
          rownames(response) <- .Call(`_Rxfr_get_bin_names`,
                                      private$client, genome,
                                      query, rowname_sep)
        } else if (is_mquery(query)) {
          stop("Cannot add rownames if query is MQuery object", call. = FALSE)
        } else { # is_intervals(query)
          rownames(response) <- .Call(`_Rxfr_get_interval_names`,
                                      query, rowname_sep)
        }
      }

      # Function to compute the number of CpG sites in each query interval
      get_n_cpgs <- function(client, genome, query) {
        if (is_bins(query)) {
          .Call(`_Rxfr_get_n_cpgs_bins`, client, genome, query)
        } else if (is_mquery(query)) {
          .Call(`_Rxfr_get_n_cpgs_query`, query)
        } else { # is_intervals(query)
          .Call(`_Rxfr_get_n_cpgs`, client, genome, query)
        }
      }

      # Add the number of CpG sites to the response
      if (add_n_cpgs) {
        n_cpgs <- get_n_cpgs(private$client, genome, query)
        if (add_header) {
          colnames(n_cpgs) <- c("N_CPGS")
        }
        response <- cbind(response, n_cpgs)
      }

      response
    },

    #' @description Create an MQuery object
    #'
    #' This method formats a set of query intervals as a MQuery object. The
    #' query intervals must be a data frame specifying chromosome, start and
    #' stop for each interval in the first three columns, respectively.
    #' Intervals are 0-based and half-open. Intervals must be sorted within
    #' each chromosome, but the order of chromosomes does not matter.
    #'
    #' @param genome The reference genome corresponding to the intervals.
    #'
    #' @param intervals (data frame) A data frame with genomic intervals
    #' specified in the first 3 columns.
    #'
    #' @export
    #' @examples
    #' # Mostly leave the config_dir empty
    #' config_xfr(c("hg38"), "usually_empty_but_in_examples")
    #'
    #' # Names of columns below are not needed
    #' intervals <- data.frame(chrom=c("chr1", "chr2", "chr3"),
    #'                         start=c(10468, 197437, 308977),
    #'                         stops=c(11240, 198535, 309210))
    #' client <- MClient$new("usually_empty_but_in_examples")
    #' query <- client$format_query("hg38", intervals)
    format_query = function(genome, intervals) {
      if (!is.data.frame(intervals)) {
        stop("intervals must be a data frame of genomic intervals",
             call. = FALSE)
      }
      MQuery$new(.Call(`_Rxfr_format_query`, private$client, genome, intervals),
                 genome, nrow(intervals))
    }
  )
)
