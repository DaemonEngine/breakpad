#!/bin/bash
set -e

srcdir=`dirname "$0"`
test -z "${srcdir}" && srcdir=.
srcdir="${srcdir}"/..
ORIGDIR=`pwd`
cd "${srcdir}"

echo fetching externals...
./fetch-externals

echo autoreconf running...
autoreconf -fvi

BUILD=$(autotools/config.guess)
if [ -z "$HOST" -o "$HOST" == "$BUILD" ] ; then
    AR="ar"
else
    AR=${HOST}-ar
    client_is_built="yes"
fi

echo configure running...
cd "$ORIGDIR"
"${srcdir}"/configure --prefix=/usr --enable-silent-rules --host=${HOST}

echo make running...
make

echo make install running...
make install DESTDIR=./staging
if [ -n "$client_is_built" ] ; then
  # Makefile made by gyp doesn't have an install target, so make up for that deficiency
  ${AR} -M <<EOF
CREATE ./staging/usr/lib/libbreakpad_client.a
ADDLIB ${srcdir}/src/out/${Configuration}/obj.target/client/windows/crash_generation/libcrash_generation_client.a
ADDLIB ${srcdir}/src/out/${Configuration}/obj.target/client/windows/crash_generation/libcrash_generation_server.a
ADDLIB ${srcdir}/src/out/${Configuration}/obj.target/client/windows/handler/libexception_handler.a
ADDLIB ${srcdir}/src/out/${Configuration}/obj.target/client/windows/libcommon.a
ADDLIB ${srcdir}/src/out/${Configuration}/obj.target/client/windows/sender/libcrash_report_sender.a
SAVE
END
EOF
  cp -a ./breakpad-client.pc ./staging/usr/lib/pkgconfig/
  cp -a ${srcdir}/src/out/${Configuration}/crash_generation_app.exe ./staging/usr/bin/
fi

echo make check running...
export PATH=/usr/${HOST}/sys-root/mingw/bin/:$PATH
make check || true
# stackwalker_mips64_unittest fails on cygwin

if [ -n "$client_is_built" ] ; then
    src/out/${Configuration}/client_tests || true
    # ExceptionHandlerDeathTest.{InvalidParam,PureVirtualCall}Test
    # ExceptionHandlerTest.{InvalidParam,PureVirtualCall}MiniDumpTest fail on MinGW
    # MinidumpTest.{Small,Larger,Full}Dump fail on MinGW x86_64
fi
