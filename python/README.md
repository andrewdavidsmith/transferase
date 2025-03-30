# pyxfr: the transferase Python package

## Installing

The pyxfr Python package allows the same queries to be done within Python as
with the transferase command line app. Almost all "client" behaviors of
transferase are available through the Python API. Installation should be
easy. You will need Python >= 3.12. Download the wheel file (`*.whl`) that
matches your system.  On Linux, there is only one and it should work on any
Linux system going back many years. On macOS, there are three for each Python
version.

Assuming you are on Linux and have Python 3.13, with the wheel file
in your current directory, just do this:

```console
# Please work in a virtual environment in case something goes wrong
python3 -m venv .venv
. .venv/bin/activate
pip install transferase-0.6.0-cp313-none-manylinux_2_17_x86_64.whl
# To test; if you see output, it worked
python3 -c "import transferase; help(transferase)"
```

On Mac, there are choices. If you happen to download and try to install the
wrong version, pip will probably tell you it's not compatible, so just try
another. If none work, let me know.

## Usage examples

See the [`docs/python.md`](../docs/python.md) in the `docs` directory of this
repo for examples showing how to use the transferase Python package.
