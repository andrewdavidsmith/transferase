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

#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace nb = nanobind;

auto
client_config_bindings(nanobind::class_<transferase::client_config> &cls)
  -> void {
  using namespace nanobind::literals;  // NOLINT
  cls.def_static(
    "default",
    []() {
      const auto sys_config_dir = transferase::find_python_sys_config_dir();
      auto client = transferase::client_config();
      std::error_code error;
      const std::string empty_config_dir;
      client.set_defaults(empty_config_dir, sys_config_dir, error);
      if (error)
        throw std::system_error(error);
      return client;
    },
    R"doc(

    Constructs a ClientConfig object with reasonable default values
    for the configuration parameters you need to interact with a
    transferase server. You can change the values afterwards, for
    example, you might want to change the log-level to see more
    information:

    >>> client = ClientConfig.default()
    >>> from transferase import LogLevel
    >>> client.log_level = LogLevel.debug

    If you use this function on an instance (client.default()) be
    aware that it will not change the calling object. You need to
    assign to an object when calling this function.

    )doc");
  cls.def("save",
          nb::overload_cast<const std::string &>(
            &transferase::client_config::save, nb::const_),
          R"doc(

    Save the configuration set in this object, writing values to files
    in the specified directory, or the default location of none is
    specified.

    Parameters
    ----------

    config_dir (str): The name of a directory to write the
        configuration. Typically this should be left as the default.

    )doc",
          "config_dir"_a = std::string());
  cls.def(
    "configure",
    [](const transferase::client_config &self,
       const std::vector<std::string> &genomes,
       const transferase::download_policy_t download_policy,
       const std::string &config_dir) {
      const auto sys_config_dir = transferase::find_python_sys_config_dir();
      self.configure(genomes, download_policy, config_dir, sys_config_dir);
    },
    R"doc(

    Does the work of configuring the client, accepting a list of
    genomes and an indicator to force redownloading. If both arguments
    are empty, the configuration will be written but no genome indexes
    will be downloaded. If you specify genomes, or request a download,
    this command will take roughly 15-30s per genome, depending on
    internet speed.

    Parameters
    ----------

    genomes (list[str]): A list of genomes, for example:
        ["hg38", "mm39", "bosTau9"]

    download_policy (DownloadPolicy): Indication of what to
        (re)download.

    config_dir (str): The name of a directory to write the
        configuration. Typically this should be left as the default.

    )doc",
    "genomes"_a = std::vector<std::string>(),
    "download_policy"_a = transferase::download_policy_t::missing,
    "config_dir"_a = std::string{});
  cls.def_rw("hostname", &transferase::client_config::hostname,
             R"doc(

    URL or IP address for the remote transferase server.  Like
    transferase.usc.edu. This must be a valid hostname. Don't specify
    a protocol or slashes, just the hostname.  You should only change
    this if there is a problem setting the server or if you have setup
    your own server.

    )doc");
  cls.def_rw("port", &transferase::client_config::port,
             R"doc(

    The server port number. You will find this along with the hostname of
    the transferase server. If it has been setup using ClientConfig, then
    you don't have to worry about it.

    )doc");
  cls.def_rw("index_dir", &transferase::client_config::index_dir,
             R"doc(

    The directory where genome index files are stored. For human and
    mouse, this occupies roughly 200MB and for all available genomes
    the total size is under 3GB. This defaults to
    '${HOME}/.config/transferase/indexes' and there is no reason to
    change it unless you are working with your own methylomes and
    started the data analysis with your own reference genome.

    )doc");
  cls.def_rw("metadata_file", &transferase::client_config::metadata_file,
             R"doc(

    This file contains information about available methylomes,
    reference genomes, and biological sample information for available
    methylomes. By default this file is pulled from a remote server
    and can be updated.  As with 'index_dir' there is no reason to
    change this unless you are working with your own data.

    )doc");
  cls.def_rw("methylome_dir", &transferase::client_config::methylome_dir,
             R"doc(

    Directory to search for methylomes stored locally.

    )doc");
  cls.def_rw("log_file", &transferase::client_config::log_file,
             R"doc(

    Log information about transferase events in this file.

    )doc");
  cls.def_rw("log_level", &transferase::client_config::log_level,
             R"doc(

    How much to log {debug, info, warning, error, critical}.

    )doc");
  cls.def("__repr__", &transferase::client_config::tostring,
          R"doc(

    Print the contents of a ClientConfig object.

    )doc");
  cls.doc() = R"doc(

    A ClientConfig object is an interface to use when setting up the
    transferase environment for the first time, or for revising the
    configuration afterwards, retrieving updated metadata, etc.

    )doc"
    //
    ;
}
