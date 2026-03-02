@echo off
REM ====================================================================
REM  Thrive Video Suite - Build Script
REM  Configures and builds the project inside the VS developer environment
REM  so that midl.exe, cl.exe, and all Windows SDK tools are available.
REM ====================================================================

REM --- Find Visual Studio ---
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set VSDIR=%%i
if not defined VSDIR (
    echo ERROR: Visual Studio not found.
    exit /b 1
)

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

REM --- Set up dependency paths ---
set DEPS=C:\dev\thrive-deps
set VCPKG_DIR=%DEPS%\vcpkg
set VCPKG_INSTALLED=%VCPKG_DIR%\installed\x64-windows
set MLT_DIR=%DEPS%\mlt
set MLT_PKG_CONFIG=%MLT_DIR%\lib\pkgconfig
set PKGCONF_BIN=%VCPKG_INSTALLED%\tools\pkgconf

REM --- Detect Qt ---
set QT_DIR=
REM Prefer MSVC builds over MinGW
for /d %%q in (C:\Qt\6.*) do (
    for /d %%a in ("%%q\msvc*") do (
        if exist "%%a\bin\qmake.exe" set QT_DIR=%%a
    )
)
if not defined QT_DIR (
    for /d %%q in (D:\Qt\6.*) do (
        for /d %%a in ("%%q\msvc*") do (
            if exist "%%a\bin\qmake.exe" set QT_DIR=%%a
        )
    )
)
REM Fallback to any Qt build (e.g. mingw) if MSVC not found
if not defined QT_DIR (
    for /d %%q in (C:\Qt\6.*) do (
        for /d %%a in ("%%q\*") do (
            if exist "%%a\bin\qmake.exe" set QT_DIR=%%a
        )
    )
)
if not defined QT_DIR (
    echo WARNING: Qt not auto-detected. Set QT_DIR manually if cmake fails.
)

REM --- Set environment ---
set CMAKE_PREFIX_PATH=%QT_DIR%;%VCPKG_INSTALLED%;%MLT_DIR%
set PKG_CONFIG_PATH=%MLT_PKG_CONFIG%;%VCPKG_INSTALLED%\lib\pkgconfig
set PATH=%MLT_DIR%\bin;%PKGCONF_BIN%;%PATH%

REM --- Configure ---
echo.
echo === Configuring Thrive Video Suite ===
echo.
cmake -B build -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%" ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows
if %ERRORLEVEL% neq 0 (
    echo.
    echo CMAKE CONFIGURE FAILED
    exit /b 1
)

REM --- Build ---
echo.
echo === Building Thrive Video Suite ===
echo.
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

echo.
echo === Build Successful! ===
echo.

REM --- Deploy Qt runtime DLLs next to the executable ---
echo === Deploying Qt runtime DLLs ===
echo.
if defined QT_DIR (
    "%QT_DIR%\bin\windeployqt.exe" --no-translations --no-system-d3d-compiler --no-opengl-sw build\thrive-video-suite.exe
    if %ERRORLEVEL% neq 0 (
        echo WARNING: windeployqt failed. You may need to add %QT_DIR%\bin to PATH.
    ) else (
        echo Qt DLLs deployed.
    )
) else (
    echo WARNING: QT_DIR not set, skipping DLL deployment.
    echo You may need to add Qt\bin to your PATH to run the app.
)

REM --- Copy MLT DLLs next to the executable ---
if exist "%MLT_DIR%\bin\libmlt-7.dll" (
    copy /y "%MLT_DIR%\bin\libmlt-7.dll" build\ >nul
    copy /y "%MLT_DIR%\bin\libmlt++-7.dll" build\ >nul
    echo MLT DLLs copied.
)

echo.
echo === Ready to run: build\thrive-video-suite.exe ===
echo.
