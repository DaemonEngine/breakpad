# Breakpad for Cygwin/MinGW

google-breakpad with added support for Cygwin/MinGW:
- A `dump_syms` tool which can read DWARF debugging information from PE/COFF executables.
- The breakpad crash-reporting client libraries built using Makefiles rather than MSVC solutions.

## Compiling

### Preparation

Run autoreconf to generate ./configure

````
autoreconf -fvi
````

### Compiling

See README.orig.md

````
./configure && make
````

will produce `dump_syms.exe`, `minidump_dump.exe`, `minidump_stackwalk.exe`, `libbreakpad.a`,
and for MinGW `libcrash_generation_client.a`, `libcrash_generation_server.a`, `crash_generation_app.exe`

Note that since git-svn ignores svn externals, this repository is missing the
gyp and gtest dependencies.

## Using

See [Getting started with breakpad](https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/getting_started_with_breakpad.md)

### Producing and installing symbols

````
dump_syms crash_generation_app.exe >crash_generation_app.sym
FILE=`head -1 crash_generation_app.sym | cut -f5 -d' '`
BUILDID=`head -1 crash_generation_app.sym | cut -f4 -d' '`
SYMBOLPATH=/symbols/${FILE}/${BUILDID}/
mkdir -p ${SYMBOLPATH}
mv crash_generation_app.sym ${SYMBOLPATH}
````

### Generating a minidump file

A small test application demonstrating out-of-process dumping called
`crash_generation_app.exe` is built.

- Run it once, selecting "Server->Start" from the menu
- Run it again, selecting "Client->Deref zero"
- Client should crash, and a .dmp is written to C:\Dumps\

### Processing the minidump to produce a stack trace

````
minidump_stackwalk blah.dmp /symbols/
````

## Issues

### Lack of build-id

On Windows, the build-id takes the form of a CodeView record.
This build-id is captured for all modules in the process by MiniDumpWriteDump(),
and is used by the breakpad minidump processing tools to find the matching
symbol file.

See http://debuginfo.com/articles/debuginfomatch.html

I have implemented 'ld --build-id' for PE/COFF executables (See
https://sourceware.org/ml/binutils/2014-01/msg00296.html), but you must use a
sufficently recent version of binutils (2.25 or later) and build with
'-Wl,--build-id' (or a gcc configured with '--enable-linker-build-id', which
turns that flag on by default) to enable that.

A tool could be written to add a build-id to existing PE/COFF executables, but in
practice this turns out to be quite tricky...

### Symbols from a PDB or the Microsoft Symbol Server

<a href="http://hg.mozilla.org/users/tmielczarek_mozilla.com/fetch-win32-symbols">
symsrv_convert</a> and dump_syms for PDB cannot be currently built with MinGW,
because (i) they require the MS DIA (Debug Interface Access) SDK (only in paid
editions of Visual Studio 2013), and (ii) the DIA SDK uses ATL.

An alternate PDB parser is available at https://github.com/luser/dump_syms, but
that also needs some work before it can be built with MinGW.

# Breakpad

Breakpad is a set of client and server components which implement a
crash-reporting system.

* [Homepage](https://chromium.googlesource.com/breakpad/breakpad/)
* [Documentation](https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/)
* [Bugs](https://bugs.chromium.org/p/google-breakpad/)
* Discussion/Questions: [google-breakpad-discuss@googlegroups.com](https://groups.google.com/d/forum/google-breakpad-discuss)
* Developer/Reviews: [google-breakpad-dev@googlegroups.com](https://groups.google.com/d/forum/google-breakpad-dev)
* Tests: [![Build Status](https://travis-ci.org/google/breakpad.svg?branch=master)](https://travis-ci.org/google/breakpad) [![Build status](https://ci.appveyor.com/api/projects/status/eguv4emv2rhq68u2?svg=true)](https://ci.appveyor.com/project/vapier/breakpad)
* Coverage [![Coverity Status](https://scan.coverity.com/projects/9215/badge.svg)](https://scan.coverity.com/projects/google-breakpad)

## Getting started (from master)

1.  First, [download depot_tools](http://dev.chromium.org/developers/how-tos/install-depot-tools)
    and ensure that they’re in your `PATH`.

2.  Create a new directory for checking out the source code (it must be named
    breakpad).

    ```sh
    mkdir breakpad && cd breakpad
    ```

3.  Run the `fetch` tool from depot_tools to download all the source repos.

    ```sh
    fetch breakpad
    cd src
    ```

4.  Build the source.

    ```sh
    ./configure && make
    ```

    You can also cd to another directory and run configure from there to build
    outside the source tree.

    This will build the processor tools (`src/processor/minidump_stackwalk`,
    `src/processor/minidump_dump`, etc), and when building on Linux it will
    also build the client libraries and some tools
    (`src/tools/linux/dump_syms/dump_syms`,
    `src/tools/linux/md2core/minidump-2-core`, etc).

5.  Optionally, run tests.

    ```sh
    make check
    ```

6.  Optionally, install the built libraries

    ```sh
    make install
    ```

If you need to reconfigure your build be sure to run `make distclean` first.

To update an existing checkout to a newer revision, you can
`git pull` as usual, but then you should run `gclient sync` to ensure that the
dependent repos are up-to-date.

## To request change review

1.  Follow the steps above to get the source and build it.

2.  Make changes. Build and test your changes.
    For core code like processor use methods above.
    For linux/mac/windows, there are test targets in each project file.

3.  Commit your changes to your local repo and upload them to the server.
    http://dev.chromium.org/developers/contributing-code
    e.g. `git commit ... && git cl upload ...`
    You will be prompted for credential and a description.

4.  At https://chromium-review.googlesource.com/ you'll find your issue listed;
    click on it, then “Add reviewer”, and enter in the code reviewer. Depending
    on your settings, you may not see an email, but the reviewer has been
    notified with google-breakpad-dev@googlegroups.com always CC’d.
