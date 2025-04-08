# Changelog

## transferase 0.6.5 (2025-04-07)

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
