# Transferase server documentation

Most of the information in this file is for admins who are installing
and running transferase on their server.

However, you can run the server just as an ordinary user, without
admin privileges on your system. I do that when running on a cluster,
because for my own local methylomes it allows me to access them very
quicly. You just need to make sure that you know the proper hostname
or IP address, and that the port is not blocked. Try some port over
5000, and run the `hostname` command to get the name to use as your
server. If you are on one system, such as an interactive session on a
cluster (even your laptop), you can just use `localhost` as the
hostname.

Installation
------------

The instructions here assume that you are installing from a package
like the STGZ, RPM or Debian package for a transferase release.  If
you build transferase from source, the installation with make or cmake
should allow you to get the same outcome. The commands will begin by
assuming the STGZ file, where the v7.7.7 is just a placeholder:

```console
sh transferase-7.7.7-Linux.sh --prefix=${PREFIX}
```

Assuming `PREFIX=/usr/local` then the following files should have been
created:

```
/usr/local
├── bin
│   ├── transferase
│   └── xfr
├── lib
│   └── systemd
│       └── system
│           └── transferase.service
└── share
    ├── bash-completion
    │   └── completions
    │       └── transferase
    └── transferase
        └── transferase_data_7.7.7.json
```

Not all of these files are needed to run the transferase server, but
one other file might be needed: a transferase server configuration
file. This is explained below in the Systemd section.

Running the transferase server
------------------------------

The easiest way to do this if you just want to check it out, is to run
it in screen or tmux and just let the log write to stdout. You need to
have some methylomes in a directory we will call `methylomes` and we
will assume you have the genome index files in a directory called
`indexes`. These files are obtained by using the format and
index commands, respectively, which are documented elsewhere.

```console
xfr server \
    -s transferase-example.net -p 5000 \
    -x indexes -d methylomes
```

This will start the server listening for requests on port 5000 of
transferase-example.net. Log messages will be written to the screen.
To stop the server, just C-c, and clients will either be notified or
timeout so they won't wait indefinitely for responses.

Certain paramaters passed to the server affect its performance and
these cannot currently be changed while the server is running. Each of
these can be set either on the command line or in the configuration
file.

- `max-resident`: The maximum number of methylomes to attempt to hold
  in memory. Each takes at most 140MB, so you can do the calculations
  for the available physical memory. But any request could involve up
  to several tens of MB to hold the query, to prepare the response,
  and to hold the response as it is being sent. Clearly the server
  responds much more quickly when the methylome is already in
  memory. Currently there is no way to adjust the policy for ejecting
  a methylome from memory when space is needed for a new one.

- `n-threads`: The maximum number of threads to use, with one thread
  per connection. Each client request, regardless of size (number of
  methylomes involved or amount of information requested per
  methylome), counts as one connection.

- `min-bin-size`: For requests related to methylation levels in
  consecutive non-overlapping genomic intervals ("bins"), this is the
  minimum bin size allowed. The smaller this value, the more intesive
  can be such requests, as they would require more information. There
  is not much reason to use a value less than the default of 100. If
  you are running this for your own personal use, then set this as low
  as you want, but be prepared for lots of useless data.

- `max-intervals`: For requests related to given sets of genomic
  intervals sent by the client. The larger this value, the larger can
  be each query, taking more time to read them and response. Currently
  the default is 2000000 if no value is specified. If you are using
  this for your own personal use, there is no issue with setting this
  as high as you want.

Systemd
-------

The simples way to run the transferase server through systemd some
minor edits to the systemd configuration file (`transferase.system`)
and some edits to the server configuration file.

If the files are in the installation directories beneath
`/usr/local` as indicated above, then certain entries should be
modified in `/usr/local/lib/systemd/system/transferase.system`.
These can be seen below, and are the `User`, the `Group` and
the `ReadWritePaths`:

```
# The "User" below needs to have transferase binary and any
# dependencies in their path, have read access to data directories,
# and write access to the log file also specified in the config file.
User=
Group=
SyslogIdentifier=transferase
# The ReadWritePaths below is likely where the log file will be
# written, but if it will go somewhere else, then update it.
ReadWritePaths=/var/tmp/
# The location of the config file below depends on the insallation
# prefix.
ExecStart=transferase server \
  --config-file=/usr/local/share/transferase/transferase_server_config.toml
```

The requirement on the user and group is that they have read and write
access to the appropriate files and directories. The
`ReadWritePaths` is where the log file will go, and this is
specified inside the transferase config file
`transferase_server_config.toml` (this can be changed, but must
exist and be readable by the chosen user).

Running detached
----------------

You can run transferase in detached mode as follows:

```console
xfr server \
  -s transferase-example.net -p 5000 \
  -x indexes -d methylomes \
  --daemon \
  --pid-file /path/to/PID_FILE \
  -v debug -l log.txt
```

Note that the running daemonized requires that a log file be specified
since the deamon has no stdout. If you use a file to store the PID
this file will be cleaned up when the server shuts down.

Server behaviors
----------------

This section gives an outline of behaviors on the part of the server
while it is receiving and responding to clients.

* Each server instance only writes to one file: a log file (e.g., in
  `/var/tmp`). The server reads files from two specified directories.
  Requests from the client to the server do not encode filesystem
  paths or filenames, nor do they encode any instructions that might
  be evaluated by a DBMS or scripting language interpreter.

* Communication from clients is in the form of requests that involve
  first a fixed sized header, followed by a varible-sized query
  payload. If request header fails to parse (by parts being
  individually invalid) or found invalid by how the parts relate to
  eachother, the server will respond asynchronously (e.g., possibly
  before the client has sent the full request) with an error message
  to the client. If this happens, the server terminates the connection
  by closing the socket. All socket read operations by the server are
  for a pre-determined numbers of bytes; no reads will continue
  "until" some pattern of types has been seen.

* Request headers are by definition exactly 512 bytes.

* Each request header has the following structure:

  - Request code (unsigned 8-bit value)
  - Data version hash (unsigned 64-bit value)
  - Auxiliary value (unsigned 32-bit value)
  - Dataset labels (text over a restricted character set)

* Request code: identifies the request type. Currently this is an
  integer in the range 0-3 inclusive and parsing will succeed for any
  8-bit unsigned integer value. Any value outside this range will
  result in a failure to validate the request and termination of the
  connection.

* Data version hash: identifies data versions to ensure the client and
  server are using compatible data. This is an arbitrary unsigned
  64-bit integer, and any value will be parsed from the request. Only
  a few dozen values for this field will correspond to data versions,
  these depend on the dataset labels part of the header. An incorrect
  hash value will result in a vailure to validate the request and
  termination of the connection.

* Auxiliary value: the purpose of this value depends on the request
  type. In general this determines the size of subsequent query data
  to be received from the client and allows for data buffers to be
  allocated dynamically to prevent overflows (and associated
  vulnerabilities). Auxiliary values are uninteger 64-bit integers,
  and any will be parsed. But these values are restricted when
  validating requests (i.e., currently between 100 and 2000000; see
  `min-bin-size` and `max-intervals` above), to prevent large queries
  that might lead to performance reduction or DoS. Any value outside
  the allowed range will result in failure to validate the request.

* Dataset labels: This is contiguous data with a unique terminator
  character. This data is composed of strings separated by a unique
  delimiter character, and each of these strings is restricted to
  characters from the set `[a-zA-Z0-9_]` (`std::alnum(c) || c == '_'`)
  No control characters or characters that might represent a relative
  or absolute directory path in a filesystem are allowed.  These
  labels are typically 10 characters each and the total space they may
  consume is limited by the total request header size of 512
  bytes. When each label is parsed from the request header, they are
  copied into dynamically sized strings. Parsing will fail if
  incorrect characters are seen here, and validation of the request as
  a whole will not be attempted. Characters or bytes that could
  potentially lead to code execution or manipulation of the system are
  rejected as early as possible.

* Beyond the header, queries involve numerical values in binary and
  might be up to megabytes in size. These numerical values are
  expected to follow specific patterns (e.g., monotonically
  increasing) but no such properties are checked. Currently if these
  values have sufficient entropy they might result in degraded
  performance of the system.  Client applications produce somewhat
  regular patterns of numbers within queries. I don't expect that any
  pattern of values in the queries could overburden the system.
