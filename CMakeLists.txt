cmake_minimum_required(VERSION 3.21)

project(ThriveVideoSuite
    VERSION 0.1.0
    DESCRIPTION "Accessible video editor for blind and visually impaired users"
    LANGUAGES CXX C
)

# ---------------------------------------------------------------------------
# Global C++ standard: C++23 everywhere by default.
# Qt-facing targets override to C++20 via set_target_properties().
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Catch unwrapped string literals – every user-facing string must go through tr()
add_compile_definitions(QT_NO_CAST_FROM_ASCII)

# ---------------------------------------------------------------------------
# Custom CMake module path
# ---------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Qt 6
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Multimedia
    LinguistTools
    Core5Compat
)

qt_standard_project_setup(
    I18N_SOURCE_LANGUAGE en
)

# ---------------------------------------------------------------------------
# MLT Framework 7 (via pkg-config)
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(MLT REQUIRED IMPORTED_TARGET mlt-framework-7)
pkg_check_modules(MLTPP REQUIRED IMPORTED_TARGET mlt++-7)

# On Windows/MSVC, MLT headers require pthread.h from pthreads-win32.
# Ensure the vcpkg include path is added to MLT's imported targets.
if(WIN32 AND DEFINED VCPKG_INSTALLED_DIR)
    set(_vcpkg_inc "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    if(EXISTS "${_vcpkg_inc}/pthread.h")
        target_include_directories(PkgConfig::MLT INTERFACE "${_vcpkg_inc}")
        target_include_directories(PkgConfig::MLTPP INTERFACE "${_vcpkg_inc}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Prism – screen reader abstraction (fetched from GitHub)
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(prism
    GIT_REPOSITORY https://github.com/ethindp/prism.git
    GIT_TAG        v0.7.1
)
set(PRISM_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(PRISM_ENABLE_DEMOS OFF CACHE BOOL "" FORCE)
set(PRISM_ENABLE_LINTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(prism)

# ---------------------------------------------------------------------------
# QuaZip – ZIP I/O for .tvs project files
# ---------------------------------------------------------------------------
set(QUAZIP_QT_MAJOR_VERSION 6 CACHE STRING "" FORCE)
FetchContent_Declare(quazip
    GIT_REPOSITORY https://github.com/stachenov/quazip.git
    GIT_TAG        v1.5
)
FetchContent_MakeAvailable(quazip)

# ---------------------------------------------------------------------------
# Sub-projects (order matters for dependency resolution)
# ---------------------------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/engine)
add_subdirectory(src/accessibility)
add_subdirectory(src/ui)
add_subdirectory(src/app)
enable_testing()
add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Internationalization – collected AFTER all targets are declared
# ---------------------------------------------------------------------------
qt_add_translations(thrive-video-suite
    TS_OUTPUT_DIRECTORY translations
    NO_GENERATE_PLURALS_TS_FILE
)

# ---------------------------------------------------------------------------
# Install rules (basic)
# ---------------------------------------------------------------------------
install(TARGETS thrive-video-suite
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
