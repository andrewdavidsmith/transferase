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

#include "client_config_bindings.hpp"

#include "bindings_utils.hpp"

#include <client_config.hpp>    // IWYU pragma: keep
#include <download_policy.hpp>  // IWYU pragma: keep

#include "listobject.h"  // for PyList_New
#include <nanobind/nanobind.h>
#include <nanobind/operators.h>   // IWYU pragma: keep
#include <nanobind/stl/string.h>  // IWYU pragma: keep
#include <nanobind/stl/vector.h>  // IWYU pragma: keep

#include <new>  // for operator new
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

auto
client_config_bindings(nanobind::class_<transferase::client_config> &cls)
  -> void {
  namespace nb = nanobind;
  namespace xfr = transferase;
  using namespace nanobind::literals;  // NOLINT
  cls.def(
    "__init__",
    [](xfr::client_config *c, const std::string &config_dir) {
      const auto sys_config_dir = xfr::find_python_sys_config_dir();
      new (c) xfr::client_config(config_dir, sys_config_dir);
    },
    R"doc(
    Constructs a MConfig object with reasonable default values for the
    configuration parameters you need to interact with a transferase
    server. You can change the values afterwards, before calling 'save' to
    write the values to the configuration file, or 'install' to create
    directories and download data needed for queries to a remote server.

    Parameters
    ----------

    config_dir (str): A directory for the location of configuration files and
        related data. The default is ok for most users.
    )doc",
    "config_dir"_a = std::string{});
  cls.def("save", nb::overload_cast<>(&xfr::client_config::save, nb::const_),
          R"doc(
    Save the configuration values associated with this object back to the
    directory associated with the calling MConfig object, which is the value
    in 'config_dir'.  The main reason to use this function is to update a
    configuration. You would first use MConfig() to load an object. Then
    modify one of the instance variables, then call 'save'.
    )doc");
  cls.def(
    "install",
    [](const xfr::client_config &self, const std::vector<std::string> &genomes,
       const std::string &download_policy) -> void {
      const auto sys_config_dir = xfr::find_python_sys_config_dir();
      const auto dlp_itr = xfr::download_policy_lookup.find(download_policy);
      if (dlp_itr == std::cend(xfr::download_policy_lookup))
        throw std::runtime_error(
          std::format("Invalid download policy: {}", download_policy));
      self.install(genomes, dlp_itr->second, sys_config_dir);
    },
    R"doc(
    Does the work related to downloading information needed by MClient
    objects.  Accepts a list of genomes and an indicator that determines what
    to download. If both arguments are empty, the configuration will be
    written but no genome indexes will be downloaded. If you specify genomes,
    or request a download, this command will take roughly 15-30s per genome,
    depending on internet speed. The configuration will be written to the
    directory associated with this object. Typically this should be left as
    the default. This command could make web requests unless 'download_policy'
    is set to 'none'.

    Parameters
    ----------

    genomes (list[str]): A list of genomes, for example: ["mm39", "bosTau9"]

    download_policy (str): Indication of what to (re)download. Possible values
    are 'none', 'missing' (get missing files), 'update' (get outdated files),
    or 'all'.
    )doc",
    "genomes"_a = std::vector<std::string>{}, "download_policy"_a = "missing");
  cls.def_rw("config_dir", &xfr::client_config::config_dir,
             R"doc(
    The directory associated with this configuration. This is either the
    directory from which this configuration was loaded, or a directory that
    has been assigned by the user. This is also the directory where this
    configuration will be written using the 'save' or 'install' functions, and
    unless you change the values, this determines the values for 'index_dir'
    (needed for both local and remote queries), along with 'methylome_dir'
    (needed for local queries).
    )doc");
  cls.def_rw("hostname", &xfr::client_config::hostname,
             R"doc(
    URL or IP address for the remote transferase server.  For example,
    'transferase.usc.edu', the public transferase server. This must be a valid
    hostname. Don't specify a protocol or slashes, just the hostname.  An IP
    address is also ok, and for some queries transferase is so fast that the
    DNS step can even cause slowdown.  You should only change this if there is
    a problem setting the server or if you have setup your own server.
    )doc");
  cls.def_rw("port", &xfr::client_config::port, R"doc(
    The server port number. You will find this along with the hostname of the
    transferase server. If it has been setup using MConfig, then you don't
    have to worry about it.
    )doc");
  cls.def_rw("index_dir", &xfr::client_config::index_dir,
             R"doc(
    The directory where genome index files are stored. For human and mouse,
    combined, this occupies roughly 200MB. For all genomes served by the
    public transferase server, the total size is under 3GB. This defaults to
    '${HOME}/.config/transferase/indexes' and there is no reason to change it
    unless you are working with your own methylomes and started the data
    analysis with your own reference genome.
    )doc");
  cls.def_rw("methbase_metadata_dataframe",
             &xfr::client_config::methbase_metadata_dataframe,
             R"doc(
    If this value is non-empty, it is the name of a file with rows
    corresponding to methylomes. This file is fetched when configuring
    transferase to use the public server. This is a dataframe/table and the
    format is 'tab-separated value'.  For each methylome in this file, the
    columns indicate summary statistics along with metadata related to the
    biological sample.
    )doc");
  cls.def_rw("methylome_dir", &xfr::client_config::methylome_dir,
             R"doc(
    Directory to search for methylomes stored locally.
    )doc");
  cls.def_rw("log_file", &xfr::client_config::log_file,
             R"doc(
    Log information about transferase events in this file.
    )doc");
  cls.def_rw("log_level", &xfr::client_config::log_level,
             R"doc(
    How much information to log or print {debug, info, warning, error,
    critical}, ordered more, to less. The default is 'info'.
    )doc");
  cls.def("__repr__", &xfr::client_config::tostring,
          R"doc(
    Print the contents of a MConfig object.
    )doc");
  cls.doc() = R"doc(
    A MConfig object provides an interface to use when "configuring", or
    setting up, the transferase environment. This must be done before first
    using transferase. It can also be done to revise the configuration
    afterwards, to retrieve updated metadata, etc. Most users will simply run:

    >>> config = MConfig()
    >>> config.install(["hg38"])

    This does a default configuration that installs everything needed to use
    the public transferase server and query the human methylomes. If you did a
    configuration using the transferase command line app, you do not need to
    repeat the process. If you want to use the public server alongside your
    own local server, for your own private data, you should run
    'MConfig(dir_name)' using two different directory names, and for each
    MConfig object, modify the instance variables accordingly.
    )doc"
    //
    ;
}
