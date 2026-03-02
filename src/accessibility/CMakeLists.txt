# ---------------------------------------------------------------------------
# thrive-accessibility  – Prism wrapper, announcer, audio cues
# ---------------------------------------------------------------------------
qt_add_library(thrive-accessibility STATIC
    screenreader.h      screenreader.cpp
    announcer.h         announcer.cpp
    accessibletimeline.h accessibletimeline.cpp
    audiocuemanager.h   audiocuemanager.cpp
)

target_include_directories(thrive-accessibility PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(thrive-accessibility
    PUBLIC
        thrive-core
        Qt6::Core
        Qt6::Multimedia
    PRIVATE
        prism
)
