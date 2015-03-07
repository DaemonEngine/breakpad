goto %1

:install
cd %APPVEYOR_BUILD_FOLDER%\..\..
appveyor DownloadFile https://storage.googleapis.com/chrome-infra/depot_tools.zip
7z -bd x depot_tools.zip -odepot_tools
call depot_tools\update_depot_tools
cd %APPVEYOR_BUILD_FOLDER%

PATH C:\projects\depot_tools;%PATH%
cd %APPVEYOR_BUILD_FOLDER%\..
call gclient config https://%APPVEYOR_REPO_PROVIDER%.com/%APPVEYOR_REPO_NAME% --unmanaged --name=src
call gclient sync
cd %APPVEYOR_BUILD_FOLDER%
goto :eof

:build_script
cd %APPVEYOR_BUILD_FOLDER%
msbuild src\client\windows\breakpad_client.sln /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll" /m /verbosity:normal
msbuild src\tools\windows\tools_windows.sln    /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll" /m /verbosity:normal
goto :eof

:test_script
src\client\windows\%CONFIGURATION%\client_tests.exe && src\tools\windows\%CONFIGURATION%\dump_syms_unittest.exe
goto :eof
