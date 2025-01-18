# Docker containers for transferase

On Ubuntu base systems, and probably most others, zlib is not
available by default so it can be installed using:
```console
apt-get install -y zlib1g-dev
```

Transferase (with binary `xfr`) is written using the C++23 standard
as implemented in GCC 14.2 and Clang 20. As of late November 2024
(time of writing), these are not available except by building from
source. The minimum version of Boost that is required is currently
1.85.0. As of late November 2024, the version available by default
with `apt` is 1.74.0. Similarly, CMake 3.30 is needed for building
transferase, and it is not easily available. The current
recommendation is to use the following docker container:
```console
andrewdavidsmith/gcc-cmake-boost
```
as it includes suitable versions of GCC, CMake and Boost. You will
need to use `apt` to get missing dependencies, including `zlib1g-dev`
for `zlib`.

To run the `transferase` server, the following must be provided to the
container:

- The methylomes directory
- The indexes directory
- The network, so the server can be reached

The following are probably desirable to provide to the container:

- A configuration file location
- A location for log file that can persist

Here is one way to accomplish the above:
```console
# assume xfr_dir has a subdir with methylomes, indexes and a config file
export TRANSFERASE_DIR=/path/to/transferase_dir
docker run \
    -v ${TRANSFERASE_DIR}:/transferase -w /transferase \
    -u $(id -u):$(id -g) --rm -it --net=host \
    xfr server -s 127.0.0.1 -p 5000 -v debug \
        -m /transferase/methylomes \
        -l /transferase/transferase_server.log \
        -c /transferase/transferase_server_config.toml
```
The network is passed to the container with the `--net=host` option to
`docker run`. The `id` command is used to set the username and group
so any files that will be created will have the right permissions.
The files will be available within the container using the
`/transferase` path. With this setup, the server log can be viewed as
follows:
```console
tail -f ${TRANSFERASE_DIR}/transferase_server.log
```
When the server is terminated (e.g., with `^C`), the log file will
remain in `/path/to/transferase_dir`.
