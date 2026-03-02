# ---------------------------------------------------------------------------
# thrive-video-suite  – main application executable
# ---------------------------------------------------------------------------
qt_add_executable(thrive-video-suite WIN32
    main.cpp
    mainwindow.h    mainwindow.cpp
    shortcutmanager.h shortcutmanager.cpp
    welcomewizard.h welcomewizard.cpp
    constants.h
)

# Pin to C++20 for Qt header compatibility
set_target_properties(thrive-video-suite PROPERTIES CXX_STANDARD 20)

target_include_directories(thrive-video-suite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(thrive-video-suite
    PRIVATE
        thrive-ui
        thrive-engine
        thrive-accessibility
        thrive-core
        Qt6::Core
        Qt6::Widgets
        Qt6::Multimedia
        prism
)

# Embed resources
qt_add_resources(thrive-video-suite "resources"
    PREFIX "/"
    BASE   "${CMAKE_SOURCE_DIR}/resources"
    FILES
        "${CMAKE_SOURCE_DIR}/resources/sounds/clip_start.wav"
        "${CMAKE_SOURCE_DIR}/resources/sounds/clip_end.wav"
        "${CMAKE_SOURCE_DIR}/resources/sounds/gap.wav"
        "${CMAKE_SOURCE_DIR}/resources/sounds/track_boundary.wav"
        "${CMAKE_SOURCE_DIR}/resources/sounds/selection.wav"
        "${CMAKE_SOURCE_DIR}/resources/sounds/error.wav"
)
