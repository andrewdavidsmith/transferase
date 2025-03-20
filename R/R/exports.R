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

config_xfr <- function(genomes, config_dir = "") {
  invisible(.Call(`_transferase_config_xfr`, genomes, config_dir))
}

set_xfr_log_level <- function(log_level) {
  invisible(.Call(`_transferase_set_xfr_log_level`, log_level))
}

get_xfr_log_level <- function() {
  .Call(`_transferase_get_xfr_log_level`)
}

init_logger <- function() {
  .Call(`_transferase_init_logger`)
}

MQuery <- R6Class(
  "MQuery",
  private = list(
    .data = NULL,
    .genome = NULL,
    .size = NA
  ),
  active = list(
    data = function(value) {
      if (missing(value)) {
        private$.data
      } else {
        stop("`$data` is read only", call. = FALSE)
      }
    },
    genome = function(value) {
      if (missing(value)) {
        private$.genome
      } else {
        stop("`$genome` is read only", call. = FALSE)
      }
    },
    size = function(value) {
      if (missing(value)) {
        private$.size
      } else {
        stop("`$size` is read only", call. = FALSE)
      }
    }
  ),
  public = list(
    initialize = function(data, genome, size) {
      private$.data <- data
      private$.genome <- genome
      private$.size <- size
    },
    print = function(...) {
      cat("MQuery:\n")
      cat("genome: ", private$.genome, "\n", sep = "")
      cat("size:   ", private$.size, "\n", sep = "")
      cat("data:   ", typeof(private$.data), "\n", sep = "")
    }
  )
)

MClient <- R6Class(
  "MClient",
  private = list(
    client = NULL
  ),
  public = list(
    config_dir = NULL,
    initialize = function(config_dir = "") {
      if (config_dir == "") {
        home_dir <- Sys.getenv("HOME")
        config_dir <- file.path(home_dir, ".config", "transferase")
      } else if (!file.exists(config_dir)) {
        fmt <- "directory does not exist: %s. See the config_xfr function"
        stop(sprintf(fmt, config_dir), call. = FALSE)
      }
      config_file <- file.path(config_dir, "transferase_client.json")
      if (!file.exists(config_file)) {
        fmt <- "config file does not exist: %s. See the config_xfr function"
        stop(sprintf(fmt, config_file), call. = FALSE)
      }
      self$config_dir <- config_dir
      private$client <- .Call(`_transferase_create_mclient`, config_dir)
    },
    print = function(...) {
      cat("MClient:\n")
      cat("config_dir: ", self$config_dir, "\n", sep = "")
      cat("client:     ", typeof(private$client), "\n", sep = "")
    },
    do_query = function(methylomes, query,
                        genome = NULL,
                        covered = FALSE,
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

      get_query_response <- function(methylomes, query, genome, covered) {
        if (is_intervals(query)) {
          if (covered) {
            .Call(`_transferase_query_intervals_cov`, private$client,
                  methylomes, genome, query)
          } else {
            .Call(`_transferase_query_intervals`, private$client,
                  methylomes, genome, query)
          }
        } else if (is_bins(query)) {
          if (covered) {
            .Call(`_transferase_query_bins_cov`, private$client,
                  methylomes, query)
          } else {
            .Call(`_transferase_query_bins`, private$client,
                  methylomes, query)
          }
        } else if (is_mquery(query)) {
          if (covered) {
            .Call(`_transferase_query_preprocessed_cov`, private$client,
                  methylomes, query$data)
          } else {
            .Call(`_transferase_query_preprocessed`, private$client,
                  methylomes, query$data)
          }
        } else {
          stop("Invalid 'query' argument", call. = FALSE)
        }
      }
      response <- get_query_response(methylomes, query, genome, covered)

      # Add a header to the results
      if (add_header) {
        sub_headings <- if (covered) c("M", "U", "C") else c("M", "U")
        header <- c(t(outer(methylomes, sub_headings, paste, sep = header_sep)))
        colnames(response) <- header
      }

      # Add rownames to the results
      if (add_rownames) {
        if (is_bins(query)) {
          rownames(response) <- .Call(`_transferase_get_bin_names`,
                                      private$client, genome,
                                      query, rowname_sep)
        } else if (is_mquery(query)) {
          stop("Cannot add rownames if query is MQuery object", call. = FALSE)
        } else { # is_intervals(query)
          rownames(response) <- .Call(`_transferase_get_interval_names`,
                                      private$client, query, rowname_sep)
        }
      }
      response
    },
    format_query = function(genome, intervals) {
      if (!is.data.frame(intervals)) {
        stop("intervals must be a data frame of genomic intervals",
             call. = FALSE)
      }
      MQuery$new(.Call(`_transferase_format_query`,
                       private$client, genome, intervals),
                 genome, nrow(intervals))
    }
  )
)
