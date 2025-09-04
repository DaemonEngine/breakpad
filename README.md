# Breakpad for MinGW and more

Google Breakpad with added support for MinGW, maintained for the
[Dæmon game engine](https://github.com/DaemonEngine/Daemon) and the
[Unvanquished game](https://unvanquished.net).

- Upstream for the Dæmon branch: https://github.com/DaemonEngine/breakpad
- Bug tracker for the Dæmon branch: https://github.com/DaemonEngine/breakpad/issues


## Dæmon Breakpad

Breakpad is a set of client and server components which implement a crash-reporting system.

The Dæmon Breakpad adds Cygwin/MinGW support to Google Breakpad, based on Jon Turney's patches,
with merged Google upstream adding support for DWARF5 debugging information format.
It retains the support for systems already supported by the Google Breakpad upstream.

It provides:

- A set of `dump_syms` tools to process debugging informations from various binary format like Linux ELF
  executables, MinGW PE/COFF executables with DWARF debug information for Windows,
  [Native Client](https://www.chromium.org/nativeclient/) binaries and others.
- The breakpad crash-reporting client libraries built using Makefiles rather than MSVC solutions.


## Compiling

### Preparation

Optional: Run the fetch-externals script to fetch submodules in the DEPS file (e.g the gyp and gtest dependencies).
(The Google upsteam repository is meant to be checked out using Chromium's `depot_tools`, which does this for you).
This is not needed to build Breakpad.

```sh
./fetch-externals
```

### Building

💡️ Instead of `-j4` you can use `-jN` where `N` is your number of CPU cores to distribute compilation on them.
Linux systems usually provide a handy `nproc` tool that tells the number of CPU core so you can just do `-j$(nproc)`
to use all available cores.

Run `autoreconf` to generate `./configure`:

```sh
autoreconf -fvi
```
Run `./configure` to configure the build:

```sh
./configure
```

Run `make` to build:

```sh
make -j4
```

This will produce `dump_syms.exe`, `minidump_dump.exe`, `minidump_stackwalk.exe`, `libbreakpad.a`,
and for MinGW `libcrash_generation_client.a`, `libcrash_generation_server.a`, `crash_generation_app.exe`

The `dump_syms` tool to process Linux binaries can be found as `src/tools/linux/dump_syms/dump_syms` and the one to process MinGW binaries can be found as `src/tools/windows/dump_syms_dwarf/dump_syms`.


## Using

See [Getting started with breakpad](https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/getting_started_with_breakpad.md) in Chromium documentation.

### Producing and installing symbols

```sh
dump_syms crash_generation_app.exe >crash_generation_app.sym
FILE=`head -1 crash_generation_app.sym | cut -f5 -d' '`
BUILDID=`head -1 crash_generation_app.sym | cut -f4 -d' '`
SYMBOLPATH=/symbols/${FILE}/${BUILDID}/
mkdir -p ${SYMBOLPATH}
mv crash_generation_app.sym ${SYMBOLPATH}
```

### Generating a minidump file

A small test application demonstrating out-of-process dumping called
`crash_generation_app.exe` is built.

- Run it once, selecting "Server->Start" from the menu
- Run it again, selecting "Client->Deref zero"
- Client should crash, and a `.dmp` is written to `C:\Dumps\`

### Processing the minidump to produce a stack trace

```sh
minidump_stackwalk blah.dmp /symbols/
```


## Issues

### Lack of build-id

On Windows, the build-id takes the form of a CodeView record.
This build-id is captured for all modules in the process by `MiniDumpWriteDump()`,
and is used by the breakpad minidump processing tools to find the matching
symbol file.

See <http://debuginfo.com/articles/debuginfomatch.html>.

I (Jon Turney) have implemented `ld --build-id` for PE/COFF executables (See
<https://sourceware.org/ml/binutils/2014-01/msg00296.html>), but you must use a
sufficently recent version of `binutils` (2.25 or later) and build with
`-Wl,--build-id`' (or a GCC configured with `--enable-linker-build-id`, which
turns that flag on by default) to enable that.

A tool could be written to add a build-id to existing PE/COFF executables, but in
practice this turns out to be quite tricky...

### Symbols from a PDB or the Microsoft Symbol Server

[`symsrv_convert`](http://hg.mozilla.org/users/tmielczarek_mozilla.com/fetch-win32-symbols")
and `dump_syms` for PDB cannot be currently built with MinGW,
because

1. they require the MS DIA (Debug Interface Access) SDK (only in paid
editions of Visual Studio 2013),
2. the DIA SDK uses ATL.

An alternate PDB parser is available at <https://github.com/luser/dump_syms>, but
that also needs some work before it can be built with MinGW.

## Upstream Breakpad

* Homepage: https://chromium.googlesource.com/breakpad/breakpad/
* Documentation: https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/
* Bugs: https://bugs.chromium.org/p/google-breakpad/
* Discussion/Questions: [google-breakpad-discuss@googlegroups.com](https://groups.google.com/d/forum/google-breakpad-discuss)
* Developer/Reviews: [google-breakpad-dev@googlegroups.com](https://groups.google.com/d/forum/google-breakpad-dev)
