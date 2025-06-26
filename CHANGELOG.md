# Changelog

## transferase 0.6.3 (2025-06-25)

Feature changes:
* Sliding windows queries are now supported.
* Better compression means faster responses for remote queries involving bins
  and sliding windows.

Changes:
* Download policy objects have been replaced by strings in pyxfr.
* More group manipulation features have been added to the select command.
* The redundant bin end value has been removed from the output format for bins
  queries.
* The JSON input format for queries has been modified.
* All forms of queries (local/remote; regular/covered) can be done with
  sliding windows.
* Responses for bins and sliding windows queries are compressed using
  libdeflate.

## transferase 0.6.2 (2025-05-10)

Feature changes:
* Queries can accept more options to customize output formats.
* Users can customize how methylome are named in query output.
* The select command now supports methylome groups defined by users.

Changes:
* Documentation for the query command has been expanded.
* LogLevel objects have been replaced by strings in the Python and R APIs.
* Bug fixes now avoid redundant downloads during user (re)configuration.
* Bug fixes allow for faster server responses.
* Incorrect error codes reported to users by the server have been fixed.
* Query output writing performance has been improved.
* Fixed bugs in horizontal scrolling within the select command.
* The transferase Dockerfile has been removed as it is unneeded.
* Fixed bug in which the server kept working on a query after seeing a
  non-existing methylome
* In the query and config commands, log levels are determined by choices of
  quiet and verbose, instead of log levels.

## transferase 0.6.1 (2025-04-07)

Feature changes:
* Added more detail about samples in the 'select' command.
* pyxfr is now on pypi and installable with pip.
* MethBase2 metadata is now available in the Python API as a pandas data frame.
* Renamed Python API to pyxfr to avoid confusion in managing which files are
  part of which package -- and easier to type.

Changes:
* Configuration file formats have changed; the system should update them as
  they are now versioned.
* In the Python and R packages, all queries must specify the genome. This was
  changed in part to simplify code and improve performance, and in part to add
  an additional confirmation of user intent.
