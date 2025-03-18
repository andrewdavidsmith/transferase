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
  invisible(.Call(`_transferase_init_logger`))
}

MQuery <- R6Class(
  "MQuery",
  private = list(
    .data = NULL
  ),
  active = list(
    data = function(value) {
      if (missing(value)) {
        private$.data
      } else {
        stop("`$data` is read only", call. = FALSE)
      }
    }
  ),
  public = list(
    initialize = function(data) {
      private$.data <- data
    },
    print = function(...) {
      cat("MQuery:\n")
      cat("data: ", typeof(private$.data), "\n", sep = "")
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
    do_query = function(methylomes, query, genome = NULL, covered = FALSE) {
      if (is.data.frame(query)) {
        if (covered) {
          .Call(`_transferase_query_intervals_cov`, private$client,
                methylomes, genome, query)
        } else {
          .Call(`_transferase_query_intervals`, private$client,
                methylomes, genome, query)
        }
      } else if (is.atomic(query) && length(query) == 1 && is.numeric(query)) {
        if (covered) {
          .Call(`_transferase_query_bins_cov`, private$client,
                methylomes, query)
        } else {
          .Call(`_transferase_query_bins`, private$client,
                methylomes, query)
        }
      } else if (class(query)[1] == "MQuery") {
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
    },
    format_query = function(genome, intervals) {
      MQuery$new(.Call(`_transferase_format_query`,
                       private$client, genome, intervals))
    }
  )
)
