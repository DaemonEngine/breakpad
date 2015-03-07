set CYGWIN_ROOT=C:\cygwin
if "%HOST%"=="x86_64-pc-cygwin" set CYGWIN_ROOT=C:\cygwin64
goto %1

:install
set CYGWIN_MIRROR=http://cygwin.mirror.constant.com
set SETUP=setup-x86.exe
if "%HOST%"=="i686-w64-mingw32" set PKGARCH=mingw64-i686
if "%HOST%"=="x86_64-w64-mingw32" set PKGARCH=mingw64-x86_64
if "%HOST%"=="i686-pc-cygwin" set SETUP=setup-x86.exe
if "%HOST%"=="x86_64-pc-cygwin" set SETUP=setup-x86_64.exe
if not defined PKGARCH (
set PACKAGES="gcc-g++,libcurl-devel,pkg-config"
) else (
set PACKAGES="%PKGARCH%-curl,%PKGARCH%-headers,%PKGARCH%-gcc-g++,%PKGARCH%-pkg-config"
)
echo Updating Cygwin and installing build dependencies
%CYGWIN_ROOT%\%SETUP% -qnNdO -R "%CYGWIN_ROOT%" -s "%CYGWIN_MIRROR%" -l "%CACHE%" -g -P "autoconf,automake,libtool,make,python2,%PACKAGES%"
goto :eof

:build_script
SET PATH=%CYGWIN_ROOT%/bin
%CYGWIN_ROOT%\bin\bash -lc "cd $APPVEYOR_BUILD_FOLDER; sh scripts/appveyor-gcc.sh"
goto :eof

:test_script
goto :eof
