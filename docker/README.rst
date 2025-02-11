Note
====

As of transferase v0.3.0, the binary distribution should run on almost
any linux system dating back at least 15 years – at least as well as it
would run through a docker container. So the approach described here
might not be extremely useful.

Docker containers for transferase
=================================

Transferase (with binary xfr) is written using C++23 as implemented
in GCC 14.2 and Clang 20. As of sometime in January 2025, GCC 14.2 is
available through apt on Ubuntu systems. Clang 20 has been available to
build nicely from source for a long time, but they seem reluctant to
release it even as a source distribution. The minimum version of Boost
that is required is currently 1.85.0. As of January 2025, Boost version
1.83 is available through apt, but transferase needs 1.85. CMake 3.30 is
needed for building transferase, and it is not easily available. The
current recommendation is to use the following docker container:

.. code:: console

   andrewdavidsmith/gcc-cmake-boost

as it includes suitable versions of GCC, CMake and Boost. You will
need to use apt to get missing dependencies, including zlib1g-dev for
zlib (``apt-get install -y zlib1g-dev``).

Documentation about how to run a transferase server can be found
elsewhere. You will need to make sure that any needed data can be
accessed through the docker container:

-  The methylomes directory
-  The indexes directory
-  The network, so the server can be reached

The following are probably desirable to provide to the container:

-  A configuration file location
-  A location for log file that can persist

Here is one way to accomplish the above. Assume ``xfr_dir`` has
subdirectories named ``methylomes``, ``indexes`` and a config file
named ``transferase_server_config.conf``:

.. code:: console

   export XFR_DIR=/path/to/xfr_dir
   docker run \
       -v ${XFR_DIR}:/transferase -w /transferase \
       -u $(id -u):$(id -g) --rm -it --net=host \
       xfr server -s 127.0.0.1 -p 5000 -v debug \
           -x /transferase/indexes \
           -d /transferase/methylomes \
           -l /transferase/transferase_server.log \
           -c /transferase/transferase_server_config.conf

The network is passed to the container with the ``--net=host`` option to
``docker run``. The ``id`` command is used to set the username and group
so any files that will be created will have the right permissions. The
files will be available within the container using the ``/transferase``
path. With this setup, the server log can be viewed as follows:

.. code:: console

   tail -f ${TRANSFERASE_DIR}/transferase_server.log

When the server is terminated (e.g., with ``^C``), the log file will
remain in ``/path/to/xfr_dir``.
