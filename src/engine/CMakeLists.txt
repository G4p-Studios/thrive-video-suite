# ---------------------------------------------------------------------------
# thrive-engine  – MLT wrapper: playback, rendering, effect catalog
# ---------------------------------------------------------------------------
qt_add_library(thrive-engine STATIC
    mltengine.h         mltengine.cpp
    playbackcontroller.h playbackcontroller.cpp
    renderengine.h      renderengine.cpp
    effectcatalog.h     effectcatalog.cpp
    tractorbuilder.h    tractorbuilder.cpp
)

target_include_directories(thrive-engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(thrive-engine
    PUBLIC
        thrive-core
        Qt6::Core
        PkgConfig::MLT
        PkgConfig::MLTPP
)
