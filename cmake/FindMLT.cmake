# ---------------------------------------------------------------------------
# FindMLT.cmake — Locate MLT Framework 7 and MLT++ *without* pkg-config
#
# Defines IMPORTED targets:
#   MLT::mlt      — MLT Framework C library  (libmlt-7)
#   MLT::mltpp    — MLT++ C++ wrapper        (libmlt++-7)
#
# Accepted hints (cache / environment variables):
#   MLT_ROOT      — Root of the MLT install tree (contains bin/, lib/, include/)
#
# On Windows the module also injects the vcpkg pthread.h include path into
# the MLT targets because the MLT headers reference <pthread.h>.
# ---------------------------------------------------------------------------
include(FindPackageHandleStandardArgs)

# -- Collect search hints ---------------------------------------------------
set(_mlt_hints "")
if(DEFINED MLT_ROOT)
    list(APPEND _mlt_hints "${MLT_ROOT}")
endif()
if(DEFINED ENV{MLT_ROOT})
    list(APPEND _mlt_hints "$ENV{MLT_ROOT}")
endif()
# Fallback locations
list(APPEND _mlt_hints
    "C:/dev/thrive-deps/mlt"
    "D:/dev/thrive-deps/mlt"
)

# -- Find libraries ----------------------------------------------------------
find_library(MLT_LIBRARY
    NAMES libmlt-7 mlt-7 mlt
    HINTS ${_mlt_hints}
    PATH_SUFFIXES lib
)

find_library(MLTPP_LIBRARY
    NAMES libmlt++-7 mlt++-7 mltpp-7 mlt++
    HINTS ${_mlt_hints}
    PATH_SUFFIXES lib
)

# -- Find include directory --------------------------------------------------
# Both framework/ and mlt++/ headers live under include/mlt-7/
find_path(MLT_INCLUDE_DIR
    NAMES framework/mlt.h
    HINTS ${_mlt_hints}
    PATH_SUFFIXES include/mlt-7
)

# -- Find DLLs (for IMPORTED_LOCATION on Windows) ---------------------------
find_file(MLT_DLL
    NAMES libmlt-7.dll
    HINTS ${_mlt_hints}
    PATH_SUFFIXES bin
)
find_file(MLTPP_DLL
    NAMES libmlt++-7.dll
    HINTS ${_mlt_hints}
    PATH_SUFFIXES bin
)

# -- Validate ----------------------------------------------------------------
find_package_handle_standard_args(MLT
    REQUIRED_VARS MLT_LIBRARY MLTPP_LIBRARY MLT_INCLUDE_DIR
    REASON_FAILURE_MESSAGE
        "MLT Framework 7 not found. Set -DMLT_ROOT=<path> or run: build setup"
)

# -- Create IMPORTED targets -------------------------------------------------
if(MLT_FOUND AND NOT TARGET MLT::mlt)

    # --- MLT C library ---
    add_library(MLT::mlt SHARED IMPORTED)
    set_target_properties(MLT::mlt PROPERTIES
        IMPORTED_IMPLIB "${MLT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MLT_INCLUDE_DIR}"
    )
    if(MLT_DLL)
        set_target_properties(MLT::mlt PROPERTIES
            IMPORTED_LOCATION "${MLT_DLL}"
        )
    endif()

    # On Windows, MLT headers include <pthread.h> which lives in vcpkg
    if(WIN32 AND DEFINED VCPKG_INSTALLED_DIR)
        set(_vcpkg_inc "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
        if(EXISTS "${_vcpkg_inc}/pthread.h")
            set_property(TARGET MLT::mlt APPEND PROPERTY
                INTERFACE_INCLUDE_DIRECTORIES "${_vcpkg_inc}"
            )
        endif()
    endif()

    # --- MLT++ C++ wrapper ---
    add_library(MLT::mltpp SHARED IMPORTED)
    set_target_properties(MLT::mltpp PROPERTIES
        IMPORTED_IMPLIB "${MLTPP_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MLT_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES MLT::mlt
    )
    if(MLTPP_DLL)
        set_target_properties(MLT::mltpp PROPERTIES
            IMPORTED_LOCATION "${MLTPP_DLL}"
        )
    endif()

endif()

mark_as_advanced(MLT_LIBRARY MLTPP_LIBRARY MLT_INCLUDE_DIR MLT_DLL MLTPP_DLL)
