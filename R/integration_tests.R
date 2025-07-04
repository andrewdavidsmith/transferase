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

library(Rxfr)

bin_size <- 100000
window_size <- 100000
window_step <- 50000
methylomes <- c("SRX3468816", "SRX3468835")

# wget https://smithlabresearch.org/transferase/other/cpgIslandExtUnmasked_hg38.bed3
intervals_file <- "cpgIslandExtUnmasked_hg38.bed3"
genome <- "hg38"

# Make sure the intervals file exists
if (!file.exists(intervals_file)) {
  stop(sprintf("Intervals file not found: %s", intervals_file), call. = FALSE)
}

set_xfr_log_level("debug")

# Make sure we have the right behavior for non-existent config dir
client <- try(MClient$new("this_should_not_exist"))
if (inherits(client, "try-error")) {
  cat("Caught appropriate error\n")
} else {
  stop("Stopping: we should have had an error")
}

# Make sure we have the right behavior for non-existent config dir
client <- try(MClient$new("."))
if (inherits(client, "try-error")) {
  cat("Caught appropriate error\n")
} else {
  stop("Stopping: we should have had an error")
}

config_xfr(c("hg38"))

client <- MClient$new()
print(client)

intervals <- read.table(intervals_file)

query <- client$format_query(genome, intervals)
print(query)

# Regular
levels <- client$do_query(methylomes, intervals, genome,
                          add_n_cpgs = TRUE, add_header = TRUE)
print(head(levels))

levels <- client$do_query(methylomes, bin_size, genome)
print(head(levels))

levels <- client$do_query(methylomes, c(window_size, window_step),
                          genome, add_rownames = TRUE)
print(head(levels))

levels <- client$do_query(methylomes, query)
print(head(levels))

# Covered
levels <- client$do_query(methylomes, intervals, genome,
                          covered = TRUE, add_header = TRUE,
                          add_rownames = TRUE)
print(head(levels))

levels <- client$do_query(methylomes, bin_size, genome,
                          covered = TRUE, add_rownames = TRUE)
print(head(levels))

levels <- client$do_query(methylomes, c(window_size, window_step), genome,
                          covered = TRUE)
print(head(levels))

levels <- client$do_query(methylomes, query, covered = TRUE)
print(head(levels))

levels <- try(client$do_query(methylomes, query,
                              covered = TRUE, add_rownames = TRUE))
if (inherits(levels, "try-error")) {
  cat("Caught appropriate error\n")
} else {
  stop("Stopping: we should have had an error")
}
