@echo off
setlocal enabledelayedexpansion
REM ====================================================================
REM  Thrive Video Suite  —  One-Command Build
REM
REM  Usage:
REM      build              Configure (if needed) and build
REM      build test         Build then run tests
REM      build run          Build then launch the app
REM      build clean        Delete the build directory
REM      build setup        Install all dependencies from scratch
REM      build reconfigure  Force a fresh CMake configure
REM      build help         Show this help
REM
REM  First-time setup: just run "build".  If dependencies are missing
REM  the script installs them automatically.  The only prerequisite
REM  you must install yourself is Qt 6 (via the Qt online installer).
REM
REM  How it works:
REM    This script detects your installed Qt, vcpkg, and MLT locations
REM    and writes them into CMakeUserPresets.json.  CMake then reads
REM    that file — no PATH hacks, no environment variables to memorise.
REM ====================================================================

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "DEPS=C:\dev\thrive-deps"
set "ACTION=%~1"

if /i "%ACTION%"=="help"   goto :help
if /i "%ACTION%"=="-h"     goto :help
if /i "%ACTION%"=="/?"     goto :help
if /i "%ACTION%"=="/h"     goto :help
if /i "%ACTION%"=="--help" goto :help

REM ====================================================================
REM  Handle "clean"
REM ====================================================================
if /i "%ACTION%"=="clean" (
    echo.
    echo Removing build directory...
    if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
    echo Done.
    goto :eof
)

REM ====================================================================
REM  Step 1 — Find Visual Studio
REM ====================================================================
echo.
echo [1/6] Looking for Visual Studio 2022...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo.
    echo ERROR: Visual Studio Installer not found.
    echo.
    echo   Install Visual Studio 2022 Community ^(free^) with the
    echo   "Desktop development with C++" workload:
    echo     https://visualstudio.microsoft.com/downloads/
    exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo.
    echo ERROR: No Visual Studio installation found.
    echo   Install Visual Studio 2022 with "Desktop development with C++".
    exit /b 1
)
echo       Found: %VSDIR%

REM Activate MSVC compiler environment (cl.exe, link.exe, ninja, etc.)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

REM ====================================================================
REM  Step 2 — Detect Qt 6
REM ====================================================================
echo [2/6] Looking for Qt 6...
set "QT_DIR="

for %%R in (C D E) do (
    for /d %%q in (%%R:\Qt\6.*) do (
        for /d %%a in ("%%q\msvc*") do (
            if exist "%%a\bin\qmake.exe" set "QT_DIR=%%a"
        )
    )
)
for /d %%q in ("%USERPROFILE%\Qt\6.*") do (
    for /d %%a in ("%%q\msvc*") do (
        if exist "%%a\bin\qmake.exe" set "QT_DIR=%%a"
    )
)
if not defined QT_DIR (
    for %%R in (C D) do (
        for /d %%q in (%%R:\Qt\6.*) do (
            for /d %%a in ("%%q\*") do (
                if exist "%%a\bin\qmake.exe" (
                    if not defined QT_DIR set "QT_DIR=%%a"
                )
            )
        )
    )
)

if not defined QT_DIR (
    echo.
    echo ERROR: Qt 6 not found.
    echo.
    echo   Install Qt 6 using the online installer:
    echo     https://www.qt.io/download-qt-installer-oss
    echo.
    echo   During installation select:
    echo     * Qt 6.8 ^(or latest 6.x^)
    echo     * MSVC 2022 64-bit
    echo     * Qt Multimedia ^(under Additional Libraries^)
    echo.
    echo   The default install path C:\Qt is searched automatically.
    echo   After installing, just run "build" again.
    exit /b 1
)
echo       Found: %QT_DIR%

REM ====================================================================
REM  Step 3 — Check / bootstrap vcpkg + pthreads
REM ====================================================================
echo [3/6] Checking vcpkg...

set "VCPKG_DIR=%DEPS%\vcpkg"
set "VCPKG_EXE=%VCPKG_DIR%\vcpkg.exe"
set "VCPKG_INSTALLED=%VCPKG_DIR%\installed\x64-windows"

if not exist "%VCPKG_EXE%" (
    echo       vcpkg not found — installing...
    if not exist "%DEPS%" mkdir "%DEPS%"
    git clone https://github.com/microsoft/vcpkg.git "%VCPKG_DIR%"
    if !ERRORLEVEL! neq 0 (
        echo.
        echo ERROR: git clone failed.  Make sure git is installed and on PATH.
        echo   https://git-scm.com/download/win
        exit /b 1
    )
    pushd "%VCPKG_DIR%"
    call bootstrap-vcpkg.bat -disableMetrics
    popd
)

REM Only need pthreads (MLT headers require pthread.h).
REM pkgconf is no longer needed — we use a native CMake find module.
if not exist "%VCPKG_INSTALLED%\include\pthread.h" (
    echo       Installing vcpkg packages...
    "%VCPKG_EXE%" install pthreads:x64-windows
    if !ERRORLEVEL! neq 0 (
        echo.
        echo ERROR: vcpkg install failed.  See errors above.
        exit /b 1
    )
) else (
    echo       Found: %VCPKG_DIR%
)

REM ====================================================================
REM  Step 4 — Check / install MLT Framework 7
REM ====================================================================
echo [4/6] Checking MLT Framework 7...

set "MLT_DIR=%DEPS%\mlt"
set "MLT_LIB=%MLT_DIR%\lib\libmlt-7.lib"
set "MLT_STAMP=%MLT_DIR%\.msvc_ok"

REM Check both the library AND the MSVC-build stamp file.
REM If the stamp is missing, the MLT++ MSVC rebuild failed on a previous
REM run and the import library has wrong (MinGW/GCC) symbol names.
set "NEED_MLT=0"
if not exist "%MLT_LIB%" set "NEED_MLT=1"
if not exist "%MLT_STAMP%" set "NEED_MLT=1"

if "!NEED_MLT!"=="1" (
    echo       MLT not found or MSVC build incomplete — running setup...
    echo.
    echo       This downloads Shotcut portable ^(~193 MB^), extracts the
    echo       prebuilt MLT libraries, and rebuilds MLT++ with MSVC.
    echo       One-time operation.
    echo.
    powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%setup_dev_env.ps1" -QtPrefix "%QT_DIR%"
    if !ERRORLEVEL! neq 0 (
        echo.
        echo ERROR: Dependency setup failed.  See errors above.
        exit /b 1
    )
    if not exist "%MLT_STAMP%" (
        echo.
        echo ERROR: MLT++ MSVC build did not complete.
        echo   The project will have ~80 linker errors without this.
        echo   Try: delete C:\dev\thrive-deps\mlt and run "build setup"
        exit /b 1
    )
) else (
    echo       Found: %MLT_DIR%
)

REM Handle explicit "setup" action — stop after deps are ready
if /i "%ACTION%"=="setup" (
    echo.
    echo === All dependencies installed. Run "build" to compile. ===
    goto :eof
)

REM ====================================================================
REM  Step 5 — Generate CMakeUserPresets.json + configure
REM ====================================================================
echo [5/6] Configuring...

REM Convert backslashes to forward slashes for CMake
set "QT_FWD=%QT_DIR:\=/%"
set "MLT_FWD=%MLT_DIR:\=/%"
set "VCPKG_FWD=%VCPKG_DIR:\=/%"
set "VCPKG_INST_FWD=%VCPKG_INSTALLED:\=/%"

REM Write the user-local preset file (not checked into git).
REM This tells CMake exactly where Qt, MLT, and vcpkg live on THIS machine.
(
echo {
echo     "version": 6,
echo     "configurePresets": [
echo         {
echo             "name": "default",
echo             "inherits": "base",
echo             "cacheVariables": {
echo                 "CMAKE_PREFIX_PATH": "%QT_FWD%;%VCPKG_INST_FWD%;%MLT_FWD%",
echo                 "CMAKE_TOOLCHAIN_FILE": "%VCPKG_FWD%/scripts/buildsystems/vcpkg.cmake",
echo                 "VCPKG_TARGET_TRIPLET": "x64-windows",
echo                 "MLT_ROOT": "%MLT_FWD%"
echo             },
echo             "environment": {
echo                 "PATH": "%MLT_FWD%/bin;%QT_FWD%/bin;$penv{PATH}"
echo             }
echo         }
echo     ],
echo     "buildPresets": [
echo         {
echo             "name": "default",
echo             "configurePreset": "default"
echo         }
echo     ],
echo     "testPresets": [
echo         {
echo             "name": "default",
echo             "configurePreset": "default",
echo             "output": { "outputOnFailure": true }
echo         }
echo     ]
echo }
) > "%SCRIPT_DIR%CMakeUserPresets.json"

set "NEED_CONFIGURE=0"
if /i "%ACTION%"=="reconfigure" set "NEED_CONFIGURE=1"
if not exist "%BUILD_DIR%\CMakeCache.txt" set "NEED_CONFIGURE=1"

if "%NEED_CONFIGURE%"=="1" (
    echo       Running cmake --preset default ...
    cmake --preset default
    if !ERRORLEVEL! neq 0 (
        echo.
        echo *** CMake configure failed ***
        echo.
        echo   Common fixes:
        echo     - Make sure Qt Multimedia is installed ^(re-run Qt installer^)
        echo     - Run "build reconfigure" after fixing issues
        echo     - Run "build clean" then "build" to start fresh
        exit /b 1
    )
    echo       Configured.
) else (
    echo       Already configured. ^(Use "build reconfigure" to redo.^)
)

REM ====================================================================
REM  Step 6 — Build
REM ====================================================================
echo [6/6] Building...

cmake --build "%BUILD_DIR%"
if %ERRORLEVEL% neq 0 (
    echo.
    echo *** Build failed ***
    exit /b 1
)

echo.
echo ============================================================
echo   Build successful!
echo ============================================================

REM ====================================================================
REM  Deploy runtime DLLs next to the executable
REM ====================================================================
if exist "%BUILD_DIR%\thrive-video-suite.exe" (
    if exist "%QT_DIR%\bin\windeployqt.exe" (
        "%QT_DIR%\bin\windeployqt.exe" --no-translations --no-system-d3d-compiler --no-opengl-sw "%BUILD_DIR%\thrive-video-suite.exe" >nul 2>&1
    )
    if exist "%MLT_DIR%\bin\libmlt-7.dll" (
        copy /y "%MLT_DIR%\bin\libmlt-7.dll" "%BUILD_DIR%\" >nul 2>&1
        copy /y "%MLT_DIR%\bin\libmlt++-7.dll" "%BUILD_DIR%\" >nul 2>&1
    )
)

REM ====================================================================
REM  Post-build actions (test / run)
REM ====================================================================
if /i "%ACTION%"=="test" (
    echo.
    echo Running tests...
    echo.
    REM The test preset inherits the configure environment (PATH with Qt/MLT)
    ctest --preset default
    set "TEST_RESULT=!ERRORLEVEL!"
    if !TEST_RESULT! neq 0 (
        echo.
        echo *** Some tests failed ***
        exit /b 1
    )
    echo.
    echo All tests passed.
    goto :eof
)

if /i "%ACTION%"=="run" (
    echo.
    echo Launching Thrive Video Suite...
    start "" "%BUILD_DIR%\thrive-video-suite.exe"
    goto :eof
)

echo.
echo   Next steps:
echo     build run     Launch the application
echo     build test    Run all unit tests
echo.
goto :eof

REM ====================================================================
:help
REM ====================================================================
echo.
echo  Thrive Video Suite - Build Script
echo  ==================================
echo.
echo  Usage:  build [command]
echo.
echo  Commands:
echo    ^(none^)       Configure ^(if needed^) and build
echo    test         Build, then run all unit tests
echo    run          Build, then launch the application
echo    clean        Delete the build directory
echo    setup        Install dependencies only ^(skip build^)
echo    reconfigure  Force a fresh CMake configure, then build
echo    help         Show this message
echo.
echo  First time?  Just run "build".  The script will automatically
echo  install vcpkg, MLT, and other C++ dependencies.
echo.
echo  Qt 6 must be installed separately via the Qt online installer:
echo    https://www.qt.io/download-qt-installer-oss
echo.
echo  All other dependencies are installed to C:\dev\thrive-deps and
echo  are reused across rebuilds.
echo.
goto :eof
