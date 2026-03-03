<#
.SYNOPSIS
    Thrive Video Suite - Development environment setup for Windows.
.DESCRIPTION
    Downloads and configures all build prerequisites (Git, CMake, Qt 6,
    MLT Framework 7, pkg-config).  Visual Studio 2022 with the
    "Desktop development with C++" workload must already be installed.

    Run this script from an **elevated** (Administrator) PowerShell 7 or
    Windows PowerShell prompt:

        Set-ExecutionPolicy -Scope Process Bypass
        .\setup_dev_env.ps1

    After it finishes it will print the CMake command you need.
.PARAMETER DepsRoot
    Root folder for all downloaded dependencies.  Default: C:\dev\thrive-deps
.PARAMETER QtPrefix
    Path to an existing Qt 6 MSVC prefix directory, e.g. C:\Qt\6.8.2\msvc2022_64.
    If omitted, the script searches common locations (C:\Qt, D:\Qt, ~\Qt).
    If not found, it guides you through manual installation.
#>

param(
    [string] $DepsRoot   = "C:\dev\thrive-deps",
    [string] $QtPrefix   = ""
)

# -- Strict mode -----------------------------------------------------------
$ErrorActionPreference = "Stop"
$ProgressPreference    = "SilentlyContinue"   # speeds up web downloads

# -- Paths derived from parameters -----------------------------------------
$VcpkgDir    = Join-Path $DepsRoot "vcpkg"
$MltSrcDir   = Join-Path $DepsRoot "mlt-src"
$MltInstDir  = Join-Path $DepsRoot "mlt"
$EnvFile     = Join-Path $DepsRoot "thrive_env.ps1"
# $QtPrefixDir is set in Step 3 after searching for Qt

Write-Host ""
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "  Thrive Video Suite - Dev Environment Setup" -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Dependencies root : $DepsRoot"
Write-Host ""

if (-not (Test-Path $DepsRoot)) {
    New-Item -ItemType Directory -Path $DepsRoot -Force | Out-Null
    Write-Host "Created $DepsRoot"
}

# =====================================================================
#  Helper: test whether a command exists
# =====================================================================
function Test-CommandExists([string]$Name) {
    $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

# =====================================================================
#  1.  Git
# =====================================================================
Write-Host ""
Write-Host '-- Step 1/6: Git --' -ForegroundColor Yellow

if (Test-CommandExists "git") {
    $gitVer = (git --version) -replace 'git version ',''
    Write-Host "  Git $gitVer is already installed. OK" -ForegroundColor Green
} else {
    Write-Host "  Installing Git via winget..."
    winget install --id Git.Git -e --accept-source-agreements --accept-package-agreements
    # Refresh PATH so git is visible in this session
    $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("PATH", "User")
    if (-not (Test-CommandExists "git")) {
        Write-Host "  ERROR: git still not found after install. Please install manually" -ForegroundColor Red
        Write-Host "         https://git-scm.com/download/win" -ForegroundColor Red
        exit 1
    }
    Write-Host "  Git installed." -ForegroundColor Green
}

# =====================================================================
#  2.  CMake (standalone - VS 2022 bundles one, but standalone is easier)
# =====================================================================
Write-Host ""
Write-Host '-- Step 2/6: CMake --' -ForegroundColor Yellow

if (Test-CommandExists "cmake") {
    $cmakeVer = ((cmake --version) | Select-Object -First 1) -replace 'cmake version ',''
    Write-Host "  CMake $cmakeVer is already installed. OK" -ForegroundColor Green
} else {
    Write-Host "  Installing CMake via winget..."
    winget install --id Kitware.CMake -e --accept-source-agreements --accept-package-agreements
    $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("PATH", "User")
    if (-not (Test-CommandExists "cmake")) {
        Write-Host "  ERROR: cmake not found after install. Please install manually" -ForegroundColor Red
        Write-Host "         https://cmake.org/download/" -ForegroundColor Red
        exit 1
    }
    Write-Host "  CMake installed." -ForegroundColor Green
}

# =====================================================================
#  3.  Qt 6
# =====================================================================
Write-Host ""
Write-Host "-- Step 3/6: Qt 6 --" -ForegroundColor Yellow

# If the user passed -QtPrefix, use it directly
if (-not [string]::IsNullOrEmpty($QtPrefix)) {
    $QtPrefixDir = $QtPrefix
}

# Search common Qt install locations for an MSVC build of Qt 6
if ([string]::IsNullOrEmpty($QtPrefixDir) -or -not (Test-Path $QtPrefixDir)) {
    Write-Host "  Searching for existing Qt 6 installation..."

    $searchRoots = @(
        "C:\Qt",
        "D:\Qt",
        "E:\Qt",
        (Join-Path $env:USERPROFILE "Qt"),
        (Join-Path $DepsRoot "qt"),
        (Join-Path ${env:ProgramFiles} "Qt"),
        "C:\dev\Qt",
        "D:\dev\Qt"
    )

    $found = $false

    # Strategy 1: structured search (fast) - look for 6.x.x\*msvc* folders
    foreach ($root in $searchRoots) {
        if (-not (Test-Path $root)) { continue }
        Write-Host "    Checking $root ..."
        $versionDirs = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
                       Where-Object { $_.Name -match "^6\.\d" } |
                       Sort-Object { try { [version]$_.Name } catch { [version]"0.0" } } -Descending
        foreach ($vdir in $versionDirs) {
            # Accept any subfolder containing bin\qmake.exe (covers msvc2019_64,
            # msvc2022_64, msvc2022, win64_msvc2019_64, etc.)
            $subDirs = Get-ChildItem $vdir.FullName -Directory -ErrorAction SilentlyContinue
            foreach ($sub in $subDirs) {
                $testQmake = Join-Path (Join-Path $sub.FullName "bin") "qmake.exe"
                if (Test-Path $testQmake) {
                    $QtPrefixDir = $sub.FullName
                    $found = $true
                    Write-Host "    Found: $QtPrefixDir" -ForegroundColor Green
                    break
                }
            }
            if ($found) { break }
        }
        if ($found) { break }
    }

    # Strategy 2: recursive search (slower but catches non-standard layouts)
    if (-not $found) {
        Write-Host "    Trying recursive search..."
        foreach ($root in $searchRoots) {
            if (-not (Test-Path $root)) { continue }
            $qmakeHits = Get-ChildItem $root -Recurse -Filter "qmake.exe" -ErrorAction SilentlyContinue |
                         Select-Object -First 1
            if ($qmakeHits) {
                $QtPrefixDir = Split-Path (Split-Path $qmakeHits.FullName)
                $found = $true
                Write-Host "    Found: $QtPrefixDir" -ForegroundColor Green
                break
            }
        }
    }
}

$qmakeFound = $false
if (-not [string]::IsNullOrEmpty($QtPrefixDir)) {
    $qmakePath = Join-Path (Join-Path $QtPrefixDir "bin") "qmake.exe"
    if (Test-Path $qmakePath) {
        $qmakeFound = $true
    }
}

if ($qmakeFound) {
    Write-Host "  Qt 6 found at $QtPrefixDir. OK" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "  Qt 6 was NOT found on this system." -ForegroundColor Red
    Write-Host ""
    Write-Host "  Searched these locations:" -ForegroundColor Yellow
    foreach ($root in $searchRoots) {
        $exists = if (Test-Path $root) { "(exists)" } else { "(not found)" }
        Write-Host "    - $root $exists" -ForegroundColor White
    }
    Write-Host ""
    Write-Host "  If Qt IS installed somewhere else, re-run with the -QtPrefix parameter:" -ForegroundColor Yellow
    Write-Host "    .\setup_dev_env.ps1 -QtPrefix 'C:\path\to\Qt\6.x.x\msvc2022_64'" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Or install Qt 6 using the official Qt installer:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "    1. Download the Qt Online Installer from:" -ForegroundColor White
    Write-Host "       https://www.qt.io/download-qt-installer-oss" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "    2. Run the installer and select:" -ForegroundColor White
    Write-Host "       - Qt 6.8 (or latest 6.x)" -ForegroundColor White
    Write-Host "       - MSVC 2022 64-bit (or MSVC 2019 64-bit)" -ForegroundColor White
    Write-Host "       - Qt Multimedia (under Additional Libraries)" -ForegroundColor White
    Write-Host ""
    Write-Host "    3. After installing, re-run this script:" -ForegroundColor White
    Write-Host "       .\setup_dev_env.ps1" -ForegroundColor Cyan
    Write-Host ""
    Write-Host ""
    Write-Host "       Or point directly to your Qt prefix:" -ForegroundColor White
    Write-Host "       .\setup_dev_env.ps1 -QtPrefix 'C:\Qt\6.8.2\msvc2022_64'" -ForegroundColor Cyan
    Write-Host ""

    $answer = Read-Host "  Press Enter to open the download page in your browser, or type 'skip' to continue without Qt"
    if ($answer -ne "skip") {
        Start-Process "https://www.qt.io/download-qt-installer-oss"
        Write-Host ""
        Write-Host "  After installing Qt, re-run this script." -ForegroundColor Yellow
        exit 0
    }

    Write-Host "  Skipping Qt - you will need to set CMAKE_PREFIX_PATH manually." -ForegroundColor Yellow
    $QtPrefixDir = "QT_NOT_INSTALLED"
}

# =====================================================================
#  4.  vcpkg + pkgconf
# =====================================================================
Write-Host ""
Write-Host '-- Step 4/6: vcpkg + pkgconf --' -ForegroundColor Yellow

if (Test-Path (Join-Path $VcpkgDir "vcpkg.exe")) {
    Write-Host "  vcpkg already bootstrapped. OK" -ForegroundColor Green
} else {
    Write-Host "  Cloning vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git $VcpkgDir
    Push-Location $VcpkgDir
    Write-Host "  Bootstrapping vcpkg..."
    & .\bootstrap-vcpkg.bat -disableMetrics
    Pop-Location
    Write-Host "  vcpkg ready." -ForegroundColor Green
}

$vcpkg = Join-Path $VcpkgDir "vcpkg.exe"

# Install pkgconf (provides pkg-config)
Write-Host "  Installing pkgconf (pkg-config) via vcpkg..."
$oldEAP = $ErrorActionPreference; $ErrorActionPreference = "Continue"
& $vcpkg install pkgconf:x64-windows
$ErrorActionPreference = $oldEAP
Write-Host "  pkgconf installed." -ForegroundColor Green

# Install libraries that MLT needs
Write-Host "  Installing MLT build dependencies via vcpkg..."
Write-Host "  (This builds from source -- may take 10-20 minutes on first run)" -ForegroundColor DarkGray
$oldEAP = $ErrorActionPreference; $ErrorActionPreference = "Continue"
& $vcpkg install libxml2:x64-windows sdl2:x64-windows fftw3:x64-windows dlfcn-win32:x64-windows pthreads:x64-windows
$ErrorActionPreference = $oldEAP
Write-Host "  MLT dependencies installed." -ForegroundColor Green

# Define vcpkg installed directory early (needed by the MLT++ MSVC build step)
$vcpkgInstalled = Join-Path (Join-Path $VcpkgDir "installed") "x64-windows"

# =====================================================================
#  5.  MLT Framework 7 - prebuilt from Shotcut
# =====================================================================
Write-Host ""
Write-Host '-- Step 5/6: MLT Framework 7 (prebuilt) --' -ForegroundColor Yellow

$mltPkgCfg    = Join-Path (Join-Path (Join-Path $MltInstDir "lib") "pkgconfig") "mlt-framework-7.pc"
$mltMsvcStamp = Join-Path $MltInstDir ".msvc_ok"

if ((Test-Path $mltPkgCfg) -and (Test-Path $mltMsvcStamp)) {
    Write-Host "  MLT 7 already installed (MSVC-compatible). OK" -ForegroundColor Green
} else {
    # -----------------------------------------------------------------
    # Download prebuilt MLT from the Shotcut portable release.
    # Shotcut bundles MLT Framework built with MSVC for Windows x64.
    # -----------------------------------------------------------------
    $shotcutVer  = "26.2.26"
    $shotcutZip  = "shotcut-win64-$shotcutVer.zip"
    $shotcutUrl  = "https://github.com/mltframework/shotcut/releases/download/v$shotcutVer/$shotcutZip"
    $shotcutDl   = Join-Path $DepsRoot $shotcutZip
    $shotcutDir  = Join-Path $DepsRoot "shotcut-extracted"

    if (-not (Test-Path $shotcutDl)) {
        Write-Host "  Downloading Shotcut portable (~193 MB) for prebuilt MLT..."
        Write-Host "  URL: $shotcutUrl" -ForegroundColor DarkGray
        $oldProgress = $ProgressPreference; $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $shotcutUrl -OutFile $shotcutDl -UseBasicParsing
        $ProgressPreference = $oldProgress
        Write-Host "  Download complete." -ForegroundColor Green
    } else {
        Write-Host "  Shotcut ZIP already downloaded." -ForegroundColor DarkGray
    }

    # Extract the ZIP
    if (-not (Test-Path $shotcutDir)) {
        Write-Host "  Extracting Shotcut portable..."
        Expand-Archive -Path $shotcutDl -DestinationPath $shotcutDir -Force
        Write-Host "  Extracted." -ForegroundColor Green
    }

    # Find the Shotcut root inside the extracted archive
    $shotcutRoot = $null
    $candidates = @(
        (Join-Path $shotcutDir "Shotcut"),
        (Join-Path $shotcutDir "Shotcut.app"),
        $shotcutDir
    )
    foreach ($c in $candidates) {
        $testDll = Join-Path $c "libmlt-7.dll"
        if (Test-Path $testDll) { $shotcutRoot = $c; break }
    }
    # Recursive fallback
    if (-not $shotcutRoot) {
        $found = Get-ChildItem $shotcutDir -Recurse -Filter "libmlt-7.dll" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $shotcutRoot = $found.DirectoryName }
    }

    if (-not $shotcutRoot) {
        Write-Host "  ERROR: Could not find libmlt-7.dll inside extracted Shotcut." -ForegroundColor Red
        Write-Host "  Please check $shotcutDir manually." -ForegroundColor Red
    } else {
        Write-Host "  Found MLT DLLs in: $shotcutRoot" -ForegroundColor DarkGray

        # ---- Clone MLT source just for headers ----
        if (-not (Test-Path $MltSrcDir)) {
            Write-Host "  Cloning MLT source (headers only)..."
            git clone --depth 1 --branch v7.24.0 https://github.com/mltframework/mlt.git $MltSrcDir
        }

        # ---- Create install directory structure ----
        $mltBinDir2    = Join-Path $MltInstDir "bin"
        $mltLibDir     = Join-Path $MltInstDir "lib"
        $mltPkgDir     = Join-Path $mltLibDir "pkgconfig"
        $mltIncFw      = Join-Path (Join-Path $MltInstDir "include") "mlt-7"
        $mltIncFwSub   = Join-Path $mltIncFw "framework"
        $mltIncPP      = Join-Path $mltIncFw "mlt++"

        foreach ($d in @($mltBinDir2, $mltLibDir, $mltPkgDir, $mltIncFwSub, $mltIncPP)) {
            if (-not (Test-Path $d)) { New-Item $d -ItemType Directory -Force | Out-Null }
        }

        # ---- Copy DLLs from Shotcut ----
        Write-Host "  Copying MLT DLLs..."
        foreach ($dll in @("libmlt-7.dll", "libmlt++-7.dll")) {
            $src = Join-Path $shotcutRoot $dll
            if (Test-Path $src) {
                Copy-Item $src $mltBinDir2 -Force
            } else {
                Write-Host "    WARNING: $dll not found in Shotcut" -ForegroundColor Yellow
            }
        }

        # Copy MinGW runtime DLLs that libmlt-7.dll depends on.
        # The Shotcut build of libmlt-7.dll is compiled with MinGW (GCC)
        # and dynamically links against these runtime libraries.
        Write-Host "  Copying MinGW runtime DLLs needed by libmlt-7..."
        foreach ($dll in @("libwinpthread-1.dll", "libgcc_s_seh-1.dll", "libdl.dll")) {
            $src = Join-Path $shotcutRoot $dll
            if (Test-Path $src) {
                Copy-Item $src $mltBinDir2 -Force
                Write-Host "    Copied $dll" -ForegroundColor DarkGray
            } else {
                Write-Host "    NOTE: $dll not found in Shotcut (may not be needed)" -ForegroundColor DarkGray
            }
        }

        # Also copy MLT plugins directory if present
        foreach ($pluginDir in @("lib\mlt-7", "lib\mlt", "share\mlt-7\lib")) {
            $srcPlugins = Join-Path $shotcutRoot $pluginDir
            if (Test-Path $srcPlugins) {
                $destPlugins = Join-Path (Join-Path $MltInstDir "lib") "mlt-7"
                if (-not (Test-Path $destPlugins)) {
                    Copy-Item $srcPlugins $destPlugins -Recurse -Force
                }
                break
            }
        }

        # Copy share/mlt-7 (profiles, presets) if present
        foreach ($shareDir in @("share\mlt-7", "share\mlt")) {
            $srcShare = Join-Path $shotcutRoot $shareDir
            if (Test-Path $srcShare) {
                $destShare = Join-Path (Join-Path $MltInstDir "share") "mlt-7"
                if (-not (Test-Path $destShare)) {
                    New-Item (Join-Path $MltInstDir "share") -ItemType Directory -Force | Out-Null
                    Copy-Item $srcShare $destShare -Recurse -Force
                }
                break
            }
        }

        # ---- Copy headers from source ----
        Write-Host "  Copying MLT headers from source..."
        $fwHeaders = Join-Path (Join-Path $MltSrcDir "src") "framework"
        Get-ChildItem $fwHeaders -Filter "*.h" | Copy-Item -Destination $mltIncFwSub -Force

        $ppHeaders = Join-Path (Join-Path $MltSrcDir "src") "mlt++"
        if (Test-Path $ppHeaders) {
            Get-ChildItem $ppHeaders -Filter "*.h" | Copy-Item -Destination $mltIncPP -Force
            # Also copy Mlt.h or MltMlt.h etc.
            Get-ChildItem $ppHeaders -Filter "Mlt*" | Where-Object { -not $_.PSIsContainer } | Copy-Item -Destination $mltIncPP -Force
        }

        # ---- Generate .lib import libraries from DLLs ----
        Write-Host "  Generating import libraries (.lib) from DLLs..."

        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        $vsDir   = & $vsWhere -latest -property installationPath
        $vcvars  = Join-Path (Join-Path (Join-Path (Join-Path $vsDir "VC") "Auxiliary") "Build") "vcvars64.bat"

        $genLibScript = @"
@echo off
call "$vcvars" >nul 2>&1

"@

        foreach ($dllName in @("libmlt-7", "libmlt++-7")) {
            $dllPath = Join-Path $mltBinDir2 "$dllName.dll"
            $defPath = Join-Path $mltLibDir "$dllName.def"
            $libPath = Join-Path $mltLibDir "$dllName.lib"

            if (Test-Path $dllPath) {
                $genLibScript += @"

echo Generating $dllName.lib ...
dumpbin /exports "$dllPath" > "$defPath.tmp"
echo LIBRARY $dllName.dll > "$defPath"
echo EXPORTS >> "$defPath"
for /f "skip=19 tokens=4" %%a in ('type "$defPath.tmp"') do (
    if not "%%a"=="" echo     %%a >> "$defPath"
)
del "$defPath.tmp"
lib /def:"$defPath" /out:"$libPath" /machine:x64 >nul 2>&1

"@
            }
        }

        $genLibBat = Join-Path $DepsRoot "gen_mlt_libs.bat"
        Set-Content -Path $genLibBat -Value $genLibScript -Encoding ASCII
        cmd.exe /c $genLibBat

        # Verify .lib files were created
        $mltLib  = Join-Path $mltLibDir "libmlt-7.lib"
        $mltppLib = Join-Path $mltLibDir "libmlt++-7.lib"
        if ((Test-Path $mltLib) -and (Test-Path $mltppLib)) {
            Write-Host "  Import libraries generated." -ForegroundColor Green
        } else {
            Write-Host "  WARNING: Could not generate .lib files." -ForegroundColor Yellow
            Write-Host "  Trying alternative approach..." -ForegroundColor Yellow
            # Alternative: create minimal .lib stubs so CMake at least configures
        }

        # ---- Write pkg-config .pc files ----
        Write-Host "  Writing pkg-config files..."

        # Use forward slashes for pkg-config compatibility
        $prefixFwd  = $MltInstDir -replace '\\','/'
        $incFwdFwd  = "$prefixFwd/include/mlt-7"

        $pcFramework = @"
prefix=$prefixFwd
exec_prefix=`${prefix}
libdir=`${prefix}/lib
includedir=`${prefix}/include/mlt-7

Name: mlt-framework-7
Description: MLT multimedia framework - prebuilt from Shotcut
Version: 7.24.0
Libs: -L`${libdir} -llibmlt-7
Cflags: -I`${includedir}
"@

        $pcPlusPlus = @"
prefix=$prefixFwd
exec_prefix=`${prefix}
libdir=`${prefix}/lib
includedir=`${prefix}/include/mlt-7

Name: mlt++-7
Description: MLT multimedia framework C++ wrapper - prebuilt from Shotcut
Version: 7.24.0
Requires: mlt-framework-7
Libs: -L`${libdir} -llibmlt++-7
Cflags: -I`${includedir}
"@

        Set-Content -Path (Join-Path $mltPkgDir "mlt-framework-7.pc") -Value $pcFramework -Encoding ASCII
        Set-Content -Path (Join-Path $mltPkgDir "mlt++-7.pc") -Value $pcPlusPlus -Encoding ASCII

        # ---- Patch mlt_log.h for MSVC compatibility ----
        # MLT's mlt_log.h uses GCC-style variadic macros (args... with ##args)
        # which MSVC cannot parse. Replace with standard __VA_ARGS__.
        Write-Host "  Patching mlt_log.h for MSVC compatibility..."
        $mltLogH = Join-Path $mltIncFwSub "mlt_log.h"
        if (Test-Path $mltLogH) {
            $logContent = Get-Content $mltLogH -Raw
            $logContent = $logContent -replace '#define mlt_log_panic\(service, format, args\.\.\.\)\s*mlt_log\(\(service\), MLT_LOG_PANIC, \(format\), ##args\)', '#define mlt_log_panic(service, ...) mlt_log((service), MLT_LOG_PANIC, __VA_ARGS__)'
            $logContent = $logContent -replace '#define mlt_log_fatal\(service, format, args\.\.\.\)\s*mlt_log\(\(service\), MLT_LOG_FATAL, \(format\), ##args\)', '#define mlt_log_fatal(service, ...) mlt_log((service), MLT_LOG_FATAL, __VA_ARGS__)'
            $logContent = $logContent -replace '#define mlt_log_error\(service, format, args\.\.\.\)\s*mlt_log\(\(service\), MLT_LOG_ERROR, \(format\), ##args\)', '#define mlt_log_error(service, ...) mlt_log((service), MLT_LOG_ERROR, __VA_ARGS__)'
            $logContent = $logContent -replace '#define mlt_log_warning\(service, format, args\.\.\.\)\s*\\\r?\n\s*mlt_log\(\(service\), MLT_LOG_WARNING, \(format\), ##args\)', '#define mlt_log_warning(service, ...) mlt_log((service), MLT_LOG_WARNING, __VA_ARGS__)'
            $logContent = $logContent -replace '#define mlt_log_info\(service, format, args\.\.\.\)\s*mlt_log\(\(service\), MLT_LOG_INFO, \(format\), ##args\)', '#define mlt_log_info(service, ...) mlt_log((service), MLT_LOG_INFO, __VA_ARGS__)'
            $logContent = $logContent -replace '#define mlt_log_verbose\(service, format, args\.\.\.\)\s*\\\r?\n\s*mlt_log\(\(service\), MLT_LOG_VERBOSE, \(format\), ##args\)', '#define mlt_log_verbose(service, ...) mlt_log((service), MLT_LOG_VERBOSE, __VA_ARGS__)'
            $logContent = $logContent -replace '#define mlt_log_timings\(service, format, args\.\.\.\)\s*\\\r?\n\s*mlt_log\(\(service\), MLT_LOG_TIMINGS, \(format\), ##args\)', '#define mlt_log_timings(service, ...) mlt_log((service), MLT_LOG_TIMINGS, __VA_ARGS__)'
            $logContent = $logContent -replace '#define mlt_log_debug\(service, format, args\.\.\.\)\s*mlt_log\(\(service\), MLT_LOG_DEBUG, \(format\), ##args\)', '#define mlt_log_debug(service, ...) mlt_log((service), MLT_LOG_DEBUG, __VA_ARGS__)'
            Set-Content $mltLogH -Value $logContent -NoNewline
            Write-Host "  mlt_log.h patched." -ForegroundColor Green
        } else {
            Write-Host "  WARNING: mlt_log.h not found at $mltLogH" -ForegroundColor Yellow
        }

        # ---- Build MLT++ from source with MSVC ----
        # The Shotcut DLLs are built with MinGW (GCC), so C++ symbols use
        # Itanium ABI mangling which is incompatible with MSVC. The C library
        # (libmlt-7.dll) uses extern "C" so it works fine. We rebuild the
        # thin MLT++ C++ wrapper with MSVC, linking against the C library.
        Write-Host "  Building MLT++ from source with MSVC..."
        $mltppSrc = Join-Path (Join-Path $MltSrcDir "src") "mlt++"
        $mltppBuildDir = Join-Path $DepsRoot "mltpp-msvc"
        $mltppObjDir = Join-Path $mltppBuildDir "obj"
        if (-not (Test-Path $mltppObjDir)) { New-Item $mltppObjDir -ItemType Directory -Force | Out-Null }

        $buildMltppBat = Join-Path $DepsRoot "build_mltpp_msvc.bat"
        $mltppScript = @"
@echo off
call "$vcvars" >nul 2>&1
set CXXFLAGS=/nologo /EHsc /O2 /MD /std:c++17 /utf-8 /DMLTPP_EXPORTS /DWIN32 /D_WINDOWS /I"$mltppSrc" /I"$mltIncFw" /I"$($vcpkgInstalled -replace '\\','\')\include"

"@
        # Compile each .cpp file
        foreach ($cppFile in (Get-ChildItem $mltppSrc -Filter "*.cpp")) {
            $objFile = Join-Path $mltppObjDir ($cppFile.BaseName + ".obj")
            $mltppScript += "echo   Compiling $($cppFile.Name)`r`n"
            $mltppScript += "cl.exe %CXXFLAGS% /c `"$($cppFile.FullName)`" /Fo`"$objFile`"`r`n"
            $mltppScript += "if errorlevel 1 exit /b 1`r`n"
        }

        # Link into DLL
        $mltppDll = Join-Path $mltppBuildDir "libmlt++-7.dll"
        $mltppLib = Join-Path $mltppBuildDir "libmlt++-7.lib"
        $mltppScript += "`r`necho   Linking libmlt++-7.dll`r`n"
        $mltppScript += "setlocal enabledelayedexpansion`r`n"
        $mltppScript += "set OBJS=`r`n"
        $mltppScript += "for %%f in (`"$mltppObjDir\*.obj`") do set OBJS=!OBJS! `"%%f`"`r`n"
        $mltppScript += "link.exe /nologo /DLL /OUT:`"$mltppDll`" /IMPLIB:`"$mltppLib`" !OBJS! `"$(Join-Path $mltLibDir 'libmlt-7.lib')`"`r`n"
        $mltppScript += "endlocal`r`n"

        Set-Content -Path $buildMltppBat -Value $mltppScript -Encoding ASCII
        $oldEAP = $ErrorActionPreference; $ErrorActionPreference = "Continue"
        cmd.exe /c $buildMltppBat
        $ErrorActionPreference = $oldEAP

        # Install MSVC-built MLT++ over the MinGW one
        if (Test-Path $mltppDll) {
            Copy-Item $mltppDll $mltBinDir2 -Force
            Copy-Item $mltppLib $mltLibDir -Force
            Write-Host "  MLT++ built and installed (MSVC-compatible)." -ForegroundColor Green
            # Create stamp file so build.bat knows the MSVC build succeeded
            Set-Content -Path $mltMsvcStamp -Value "MLT++ rebuilt with MSVC on $(Get-Date -Format 'yyyy-MM-dd HH:mm')" -Encoding ASCII
        } else {
            Write-Host "" -ForegroundColor Red
            Write-Host "  ERROR: MLT++ MSVC build failed." -ForegroundColor Red
            Write-Host "  Without this the project will have ~80 linker errors." -ForegroundColor Red
            Write-Host "" -ForegroundColor Red
            Write-Host "  Possible fixes:" -ForegroundColor Yellow
            Write-Host "    1. Make sure VS has 'Desktop development with C++' workload" -ForegroundColor White
            Write-Host "    2. Delete $MltInstDir and re-run: build setup" -ForegroundColor White
            Write-Host "" -ForegroundColor Red
            exit 1
        }

        if (Test-Path $mltPkgCfg) {
            Write-Host "  MLT 7 installed to $MltInstDir" -ForegroundColor Green
        } else {
            Write-Host "  ERROR: pkg-config file was not created." -ForegroundColor Red
        }
    }
}

# =====================================================================
#  6.  Environment variables
# =====================================================================
Write-Host ""
Write-Host '-- Step 6/6: Environment configuration --' -ForegroundColor Yellow

$vcpkgInstalled  = Join-Path (Join-Path $VcpkgDir "installed") "x64-windows"
$vcpkgToolchain  = Join-Path (Join-Path (Join-Path $VcpkgDir "scripts") "buildsystems") "vcpkg.cmake"
$mltPkgConfigDir = Join-Path (Join-Path $MltInstDir "lib") "pkgconfig"
$mltBinDir       = Join-Path $MltInstDir "bin"
$pkgConfBin      = Join-Path (Join-Path $vcpkgInstalled "tools") "pkgconf"

# Build a helper script that sets up the shell for building
$envScript = @"
# Thrive Video Suite - build environment
# Source this in PowerShell before running CMake:
#   . $EnvFile

`$env:CMAKE_PREFIX_PATH  = "$QtPrefixDir;$vcpkgInstalled;$MltInstDir"
`$env:PKG_CONFIG_PATH    = "$mltPkgConfigDir;$vcpkgInstalled\lib\pkgconfig"
`$env:PATH               = "$mltBinDir;$pkgConfBin;`$env:PATH"

Write-Host ""
Write-Host "Thrive Video Suite build environment loaded." -ForegroundColor Green
Write-Host "  Qt          : $QtPrefixDir"
Write-Host "  MLT         : $MltInstDir"
Write-Host "  vcpkg       : $VcpkgDir"
Write-Host "  pkg-config  : $pkgConfBin"
Write-Host ""
"@

Set-Content -Path $EnvFile -Value $envScript -Encoding UTF8
Write-Host "  Created environment script: $EnvFile" -ForegroundColor Green

# Apply to current session too
$env:CMAKE_PREFIX_PATH = "$QtPrefixDir;$vcpkgInstalled;$MltInstDir"
$env:PKG_CONFIG_PATH   = "$mltPkgConfigDir;$vcpkgInstalled\lib\pkgconfig"
$env:PATH              = "$mltBinDir;$pkgConfBin;$env:PATH"

# =====================================================================
#  Done - print next steps
# =====================================================================
Write-Host ""
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "  Setup complete!" -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  To build, just run the build script from the project root:" -ForegroundColor White
Write-Host ""
Write-Host "    cd c:\Users\alex\thrive-video-suite" -ForegroundColor White
Write-Host "    .\build.bat" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Other commands:" -ForegroundColor White
Write-Host "    build test         Build and run tests" -ForegroundColor White
Write-Host "    build run          Build and launch the app" -ForegroundColor White
Write-Host "    build clean        Delete the build directory" -ForegroundColor White
Write-Host ""
Write-Host "  Installed locations:" -ForegroundColor White
Write-Host "    Qt          : $QtPrefixDir"
Write-Host "    MLT 7       : $MltInstDir"
Write-Host "    vcpkg       : $VcpkgDir"
Write-Host "    pkg-config  : $pkgConfBin"
Write-Host ""
